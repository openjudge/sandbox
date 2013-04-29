/*******************************************************************************
 * Copyright (C) 2004-2009, 2011-2013 LIU Yu, pineapple.liu@gmail.com          *
 * All rights reserved.                                                        *
 *                                                                             *
 * Redistribution and use in source and binary forms, with or without          *
 * modification, are permitted provided that the following conditions are met: *
 *                                                                             *
 * 1. Redistributions of source code must retain the above copyright notice,   *
 *    this list of conditions and the following disclaimer.                    *
 *                                                                             *
 * 2. Redistributions in binary form must reproduce the above copyright        *
 *    notice, this list of conditions and the following disclaimer in the      *
 *    documentation and/or other materials provided with the distribution.     *
 *                                                                             *
 * 3. Neither the name of the author(s) nor the names of its contributors may  *
 *    be used to endorse or promote products derived from this software        *
 *    without specific prior written permission.                               *
 *                                                                             *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE   *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  *
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE    *
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR         *
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF        *
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN     *
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)     *
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE  *
 * POSSIBILITY OF SUCH DAMAGE.                                                 *
 ******************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* need this to activate pthread_sigqueue() */
#endif /* _GNU_SOURCE */

#include "platform.h"
#include "sandbox.h"
#include "internal.h"
#include "config.h"

#include <ctype.h>              /* toupper() */
#include <fcntl.h>              /* open(), close(), O_RDONLY */
#include <math.h>               /* modfl(), lrintl() */
#include <pthread.h>            /* pthread_{create,join,...}() */
#include <signal.h>             /* kill(), SIG* */
#include <stdio.h>              /* read(), sscanf(), sprintf() */
#include <stdlib.h>             /* malloc(), free() */
#include <string.h>             /* memset(), strsep() */
#include <sys/queue.h>          /* SLIST_*() */
#include <sys/times.h>          /* struct tms, struct timeval */
#include <sys/types.h>          /* off_t */
#include <sys/wait.h>           /* waitpid(), WNOHANG */
#include <unistd.h>             /* access(), lseek(), {R,F}_OK */

#ifdef HAVE_PTRACE
#ifdef HAVE_SYS_PTRACE_H
#include <sys/ptrace.h>         /* ptrace(), PTRACE_* */
#endif /* HAVE_SYS_PTRACE_H */
#endif /* HAVE_PTRACE */

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>            /* statfs() */
#endif /* HAVE_SYS_VFS_H */

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>          /* statfs() */
#endif /* HAVE_SYS_PARAM_H */

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif /* HAVE_SYS_MOUNT_H */

#ifdef HAVE_LINUX_MAGIC_H
#include <linux/magic.h>        /* PROC_SUPER_MAGIC */
#else
#ifndef PROC_SUPER_MAGIC
#define PROC_SUPER_MAGIC 0x9fa0
#endif /* PROC_SUPER_MAGIC */
#endif /* HAVE_LINUX_MAGIC_H */

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef HAVE_PROCFS
#ifndef PROCFS
#define PROCFS "/proc"
#endif /* PROCFS */
static bool check_procfs(pid_t);
#else
#error "some functions of libsandbox require procfs"
#endif /* HAVE_PROCFS */

typedef enum
{
    T_OPTION_NOP = 0, 
    T_OPTION_ACK = 1, 
    T_OPTION_END = 2, 
    T_OPTION_NEXT = 3, 
    T_OPTION_GETREGS = 4, 
    T_OPTION_GETDATA = 5, 
    T_OPTION_GETSIGINFO = 6, 
    T_OPTION_SETREGS = 7,
    T_OPTION_SETDATA = 8,
} option_t;

static long __trace(option_t, proc_t * const, void * const, long * const);

bool
proc_bind(const void * const dummy, proc_t * const pproc)
{
    FUNC_BEGIN("%p,%p", dummy, pproc);
    assert(dummy && pproc);
    
    if ((dummy == NULL) || (pproc == NULL))
    {
        FUNC_RET("%d", false);
    }
    sandbox_t * psbox = (sandbox_t *)dummy;
    
    pproc->pid = psbox->ctrl.pid;
    pproc->tflags.trace_id = psbox->ctrl.tracer.tid;
    
    FUNC_RET("%d", true);
}

bool 
proc_probe(pid_t pid, int opt, proc_t * const pproc)
{
    FUNC_BEGIN("%d,%d,%p", pid, opt, pproc);
    assert(pproc);
    
    if (pproc == NULL)
    {
        FUNC_RET("%d", false);
    }
    
#ifdef HAVE_PROCFS
    /* Grab preliminary information from procfs */
    
    if (!check_procfs(pid))
    {
        WARN("procfs entries missing or invalid");
        FUNC_RET("%d", false);
    }
    
    char buffer[4096];
    sprintf(buffer, PROCFS "/%d/stat", pid);
   
    int fd = open(buffer, O_RDONLY);
    if (fd < 0)
    {
        WARN("procfs stat missing or unaccessable");
        FUNC_RET("%d", false);
    }
    
    int len = read(fd, buffer, sizeof(buffer) - 1);
    int errnum = errno;
    
    close(fd);

    if (len < 0)
    {
        errno = errnum;
        WARN("failed to grab stat from procfs");
        FUNC_RET("%d", false);
    }

    buffer[len] = '\0';
    
    /* Extract interested information */
    struct tms tm = (struct tms){0, 0};
    int offset = 0;
    char * token = buffer;
    do
    {
        errno = 0;
        switch (offset++)
        {
        case  0:                /* pid */
            pproc->pid = atoi(token);
            break;
        case  1:                /* comm */
            break;
        case  2:                /* state */
            pproc->state = token[0];
            break;
        case  3:                /* ppid */
            pproc->ppid = atoi(token);
            break;
        case  4:                /* pgrp */
        case  5:                /* session */
        case  6:                /* tty_nr */
        case  7:                /* tty_pgrp */
            break;
        case  8:                /* flags */
            pproc->flags = strtoul(token, NULL, 10);
            break;
        case  9:                /* min_flt */
            pproc->minflt = strtoul(token, NULL, 10);
            break;
        case 10:                /* cmin_flt */
            break;
        case 11:                /* maj_flt */
            pproc->majflt = strtoul(token, NULL, 10);
            break;
        case 12:                /* cmaj_flt */
            break;
        case 13:                /* utime */
            tm.tms_utime = strtoul(token, NULL, 10);
            break;
        case 14:                /* stime */
            tm.tms_stime = strtoul(token, NULL, 10);
            break;
        case 15:                /* cutime */
        case 16:                /* cstime */
        case 17:                /* priority */
        case 18:                /* nice */
        case 19:                /* num_threads (since 2.6 kernel) */
        case 20:                /* it_real_value */
        case 21:                /* start_time */
            break;
        case 22:                /* vsize */
            pproc->vsize = strtoul(token, NULL, 10);
            break;
        case 23:                /* rss */
            pproc->rss = strtol(token, NULL, 10);
            break;
        case 24:                /* rlim_rss */
        case 25:                /* start_code */
            pproc->start_code = strtoul(token, NULL, 10);
            break;
        case 26:                /* end_code */
            pproc->end_code = strtoul(token, NULL, 10);
            break;
        case 27:                /* start_stack */
            pproc->start_stack = strtoul(token, NULL, 10);
            break;
        case 28:                /* esp */
        case 29:                /* eip */
        case 30:                /* pending_signal */
        case 31:                /* blocked_signal */
        case 32:                /* sigign */
        case 33:                /* sigcatch */
        case 34:                /* wchan */
        case 35:                /* nswap */
        case 36:                /* cnswap */
        case 37:                /* exit_signal */
        case 38:                /* processor */
        case 39:                /* rt_priority */
        case 40:                /* policy */
        default:
            break;
        }
        if (errno != 0)
        {
            WARN("failed to parse stat from procfs");
            FUNC_RET("%d", false);
        }
    } while (strsep(&token, " ") != NULL);
    
    long CLK_TCK = sysconf(_SC_CLK_TCK);
    
    #define TS_UPDATE_CLK(pts,clk) \
    {{{ \
        (pts)->tv_sec = ((time_t)((clk) / CLK_TCK)); \
        (pts)->tv_nsec = (1000000000UL * ((clk) % (CLK_TCK)) / CLK_TCK ); \
    }}} /* TS_UPDATE_CLK */
    
    TS_UPDATE_CLK(&pproc->utime, tm.tms_utime);
    TS_UPDATE_CLK(&pproc->stime, tm.tms_stime);
    
    DBUG("proc.pid                    % 10d", pproc->pid);
    DBUG("proc.ppid                   % 10d", pproc->ppid);
    DBUG("proc.state                           %c", pproc->state);
    DBUG("proc.flags          0x%016lx", pproc->flags);
    DBUG("proc.utime                  %010lu", ts2ms(pproc->utime));
    DBUG("proc.stime                  %010lu", ts2ms(pproc->stime));
    DBUG("proc.minflt                 %010lu", pproc->minflt);
    DBUG("proc.majflt                 %010lu", pproc->majflt);
    DBUG("proc.vsize                  %010lu", pproc->vsize);
    DBUG("proc.rss                    % 10ld", pproc->rss);
    
#else
#warning "proc_probe() requires procfs"
#endif /* HAVE_PROCFS */
    
    /* Inspect process registers */
    if (opt & PROBE_REGS)
    {
        if (__trace(T_OPTION_GETREGS, pproc, NULL, NULL) != 0)
        {
            FUNC_RET("%d", false);
        }
#ifdef __x86_64__
        DBUG("regs.r15            0x%016lx", pproc->regs.r15);
        DBUG("regs.r14            0x%016lx", pproc->regs.r14);
        DBUG("regs.r13            0x%016lx", pproc->regs.r13);
        DBUG("regs.r12            0x%016lx", pproc->regs.r12);
        DBUG("regs.rbp            0x%016lx", pproc->regs.rbp);
        DBUG("regs.rbx            0x%016lx", pproc->regs.rbx);
        DBUG("regs.r11            0x%016lx", pproc->regs.r11);
        DBUG("regs.r10            0x%016lx", pproc->regs.r10);
        DBUG("regs.r9             0x%016lx", pproc->regs.r9);
        DBUG("regs.r8             0x%016lx", pproc->regs.r8);
        DBUG("regs.rax            0x%016lx", pproc->regs.rax);
        DBUG("regs.rcx            0x%016lx", pproc->regs.rcx);
        DBUG("regs.rdx            0x%016lx", pproc->regs.rdx);
        DBUG("regs.rsi            0x%016lx", pproc->regs.rsi);
        DBUG("regs.rdi            0x%016lx", pproc->regs.rdi);
        DBUG("regs.orig_rax       0x%016lx", pproc->regs.orig_rax);
        DBUG("regs.rip            0x%016lx", pproc->regs.rip);
        DBUG("regs.cs             0x%016lx", pproc->regs.cs);
        DBUG("regs.eflags         0x%016lx", pproc->regs.eflags);
        DBUG("regs.rsp            0x%016lx", pproc->regs.rsp);
        DBUG("regs.ss             0x%016lx", pproc->regs.ss);
        DBUG("regs.fs_base        0x%016lx", pproc->regs.fs_base);
        DBUG("regs.gs_base        0x%016lx", pproc->regs.gs_base);
        DBUG("regs.ds             0x%016lx", pproc->regs.ds);
        DBUG("regs.es             0x%016lx", pproc->regs.es);
        DBUG("regs.fs             0x%016lx", pproc->regs.fs);
        DBUG("regs.gs             0x%016lx", pproc->regs.gs);
#else /* __i386__ */
        DBUG("regs.ebx            0x%016lx", pproc->regs.ebx);
        DBUG("regs.ecx            0x%016lx", pproc->regs.ecx);
        DBUG("regs.edx            0x%016lx", pproc->regs.edx);
        DBUG("regs.esi            0x%016lx", pproc->regs.esi);
        DBUG("regs.edi            0x%016lx", pproc->regs.edi);
        DBUG("regs.ebp            0x%016lx", pproc->regs.ebp);
        DBUG("regs.eax            0x%016lx", pproc->regs.eax);
        DBUG("regs.xds            0x%016lx", pproc->regs.xds);
        DBUG("regs.xes            0x%016lx", pproc->regs.xes);
        DBUG("regs.xfs            0x%016lx", pproc->regs.xfs);
        DBUG("regs.xgs            0x%016lx", pproc->regs.xgs);
        DBUG("regs.orig_eax       0x%016lx", pproc->regs.orig_eax);
        DBUG("regs.eip            0x%016lx", pproc->regs.eip);
        DBUG("regs.xcs            0x%016lx", pproc->regs.xcs);
        DBUG("regs.eflags         0x%016lx", pproc->regs.eflags);
        DBUG("regs.esp            0x%016lx", pproc->regs.esp);
        DBUG("regs.xss            0x%016lx", pproc->regs.xss);
#endif /* __x86_64__ */
    }
    
    /* Inspect current instruction */
    if (opt & PROBE_OP)
    {
        unsigned long addr = pproc->regs.NIP;
        if (!pproc->tflags.single_step)
        {
            addr -= 2; /* backoff 2 bytes in system call tracing mode */
        }
        
        if (!proc_dump(pproc, (void *)addr, sizeof(long), (char *)&pproc->op))
        {
            FUNC_RET("%d", false);
        }
        
        DBUG("proc.op             0x%016lx", pproc->op);
    }
    
    /* Inspect signal information */
    if (opt & PROBE_SIGINFO)
    {
        if (__trace(T_OPTION_GETSIGINFO, pproc, NULL, NULL) != 0)
        {
            FUNC_RET("%d", false);
        }

        DBUG("proc.siginfo.si_signo       % 10d", pproc->siginfo.si_signo);
        DBUG("proc.siginfo.si_errno       % 10d", pproc->siginfo.si_errno);
        DBUG("proc.siginfo.si_code        % 10d", pproc->siginfo.si_code);
    }
    
    FUNC_RET("%d", true);
}

int
proc_abi(proc_t * const pproc)
{
    FUNC_BEGIN("%p", pproc);
    assert(pproc);
    
    if (pproc == NULL)
    {
        FUNC_RET("%d", SCMODE_MAX);
    }

#ifdef __x86_64__
    /* INT80 and SYSENTER always maps in 32bit syscall table regardless of the 
     * value of CS c.f. http://scary.beasts.org/security/CESA-2009-001.html */
    #define SYSCALL_MODE(pproc) \
        RVAL_IF((OPCODE16((pproc)->op) == OP_INT80) || \
                (OPCODE16((pproc)->op) == OP_SYSENTER)) \
            SCMODE_LINUX32 \
        RVAL_ELSE \
            RVAL_IF(OPCODE16((pproc)->op) == OP_SYSCALL) \
                RVAL_IF(((pproc)->regs.cs) == 0x23) \
                    SCMODE_LINUX32 \
                RVAL_ELSE \
                    RVAL_IF(((pproc)->regs.cs) == 0x33) \
                        SCMODE_LINUX64 \
                    RVAL_ELSE \
                        SCMODE_MAX \
                    RVAL_FI \
                RVAL_FI \
            RVAL_ELSE \
                SCMODE_MAX \
            RVAL_FI \
        RVAL_FI \
    /* SYSCALL_MODE */
#else /* __i386__ */
    #define SYSCALL_MODE(pproc) \
        RVAL_IF((((pproc)->regs.xcs) == 0x23) || \
                (((pproc)->regs.xcs) == 0x73)) \
            SCMODE_LINUX32 \
        RVAL_ELSE \
            SCMODE_MAX \
        RVAL_FI \
    /* SYSCALL_MODE */
#endif /* __x86_64__ */

    /* In single step tracing mode, we inspect system call mode from i) the 
     * instruction addressed by eip / rip and ii) the value of xcs / cs.  */
    if (pproc->tflags.single_step)
    {
        FUNC_RET("%d", SYSCALL_MODE(pproc));
    }
    
#ifdef WITH_VSYSCALL_INSPECT
#warning "vsyscall opcode inspection is an experimental feature"
#ifdef __x86_64__
    
    /* In system call tracing mode, the process may somehow stop at the 
     * entrance of __kernel_vsyscall (i.e. JMP <__kernel_vsyscall+3>), 
     * rather than at the actual system call instruction (e.g. SYSENTER).
     * We observed this issue when running static ELF-i386 programs on 
     * kernel-2.6.32-220.7.1.el6.x86_64. Perhaps a bug in the way the Linux 
     * kernel handles ptrace(). As a workaround, we can follow the JMP, and
     * try to locate the actual syscall instruction. In its current status,
     * we have only implemented JMP REL (i.e. EB and E9) decoding. Other 
     * types of JMP / CALL instructions are treated as illegal scmode. */
    
    unsigned long addr = pproc->regs.rip;
    union
    {
        unsigned long data;
        char byte[sizeof(unsigned long)];
    } code = {pproc->op};
    
    switch (OPCODE16(code.data) & 0xffUL)
    {
    case 0xeb: /* jmp rel short */
        addr += (char)((code.data & 0xff00UL) >> 8);
        break;
    case 0xe9: /* jmp rel */
        addr += (int)((code.data & 0xffffffff00UL) >> 8);
        break;
    case 0xea: /* jmp far */
    case 0xff: /* call */
    default:
        FUNC_RET("%d", SYSCALL_MODE(pproc));
        break;
    }
    
    /* The dump may fail on some ELF-i386 programs SIGKILLed during syscall.
     * We have only observed this very rare failure when inspecting ELF-i386
     * programs on a x86_64 platform running 2.6 kernel. Seems like the JMP
     * lead us to some mythical place that is not observable. Could it be that
     * the vsyscall page in just got unmapped upon SIGKILL? Good news is that,
     * as far as we have observed, such failures are unique to 2.6 kernels. */
    
    if (!proc_dump(pproc, (void *)addr, sizeof(code), code.byte))
    {
        /* Fallback to the straightforward __trace() and see if we got luck */
        if (__trace(T_OPTION_GETDATA, (proc_t *)pproc, (void *)addr,
                    (long *)&code.data) != 0)
        {
            WARN("failed to dump vsyscall page for inspection");
            FUNC_RET("%d", SCMODE_MAX);
        }
    }
    
    /* The code now contains the initial instructions from the vsyscall entry.
     * We assume no further JMP's before reaching the real syscall instruction,
     * otherwise, we will have to implement a full x86_64 disassembler :-( */
    size_t offset;
    for (offset = 0; offset < sizeof(long) - 1; offset++)
    {
        if ((OPCODE16(code.data) == OP_INT80) || \
            (OPCODE16(code.data) == OP_SYSENTER) || \
            (OPCODE16(code.data) == OP_SYSCALL))
        {
            /* patch rip and refill opcode in the process stat buffer */
            addr += offset;
            if (!proc_dump(pproc, (void *)addr, sizeof(code), code.byte))
            {
                /* This is not a failure since we have aleady obtained a piece
                 * of vsyscall implementation in code, just that pproc->op may
                 * contain zero paddings introduced during the inspection */
                WARN("failed to dump vsyscall page for opcode refill");
            }
            pproc->regs.rip = addr;
            pproc->op = code.data;
            DBUG("vsyscall_addr       0x%016lx", addr);
            DBUG("vsyscall_code       0x%016lx", code.data);
            break;
        }
        code.data = (code.data >> 8);
    }
    
#endif /* __x86_64__ */
#endif /* WITH_VSYSCALL_INSPECT */
    
    FUNC_RET("%d", SYSCALL_MODE(pproc));
}

bool
proc_dump(const proc_t * const pproc, const void * const addr, 
          size_t len, char * const buff)
{
    FUNC_BEGIN("%p,%p,%zu,%p", pproc, addr, len, (void * const)buff);
    assert(pproc && (addr || true) && (len > 0) && buff);
    
    if ((pproc == NULL) || (addr == NULL) || (len <= 0) || (buff == NULL))
    {
        errno = (addr == NULL) ? (EIO) : (EINVAL);
        FUNC_RET("%d", false);
    }
    
    /* Adjust address when it is not word-aligned. Also take care of some
     * exceptional conditions. The following method was mostly learned from
     * the umoven() function from strace-4.4.98 (util.c) */
    #ifndef MIN
    #define MIN(a,b) (((a) < (b)) ? (a) : (b))
    #endif
    
    unsigned long src = (unsigned long)addr;
    char * dest = buff;
    bool dumped = false;
    
    union
    {
        unsigned long data;
        char byte[sizeof(unsigned long)];
    } word;
    
    if (src & (sizeof(word) - 1))
    {
        ssize_t start = src - (src & -sizeof(word));
        src &= -sizeof(word);
        if (__trace(T_OPTION_GETDATA, (proc_t *)pproc, (void *)src, 
                    (long *)&word.data) != 0)
        {
            FUNC_RET("%d", false);
        }
        ssize_t count = MIN(sizeof(word) - start, len);
        memcpy(dest, &word.byte[start], count);
        DBUG("data.%p     0x%016lx", (void *)src, word.data);
        dumped = true;
        src += sizeof(word);
        dest += count;
        len -= count;
    }
    
    while (len > 0)
    {
        if (__trace(T_OPTION_GETDATA, (proc_t *)pproc, (void *)src, 
                    (long *)&word.data) != 0)
        {
            /* Reached 'end of memory', but alreay dumped some memory blocks */
            if (dumped && ((errno == EPERM) || (errno == EIO)))
            {
                errno = EFAULT;
                FUNC_RET("%d", false);
            }
            FUNC_RET("%d", false);
        }
        ssize_t count = MIN(sizeof(word), len);
        memcpy(dest, word.byte, count);
        DBUG("data.%p     0x%016lx", (void *)src, word.data);
        dumped = true;
        src += sizeof(word);
        dest += count;
        len -= count;
    }
    
#ifdef DELETED
#ifdef HAVE_PROCFS
    /* Access the memory of the prisoner process via procfs */
    char buffer[4096];

    sprintf(buffer, PROCFS "/%d/mem", pproc->pid);
    if (access(buffer, R_OK | F_OK) < 0)
    {
        WARN("procfs entries missing or invalid");
        FUNC_RET("%d", false);
    }

    /* Copy a word from the specified address */
    int fd = open(buffer, O_RDONLY);
    if (lseek(fd, (off_t)addr, SEEK_SET) < 0)
    {
        extern int errno;
        WARN("lseek(%d, %p, SEEK_SET) failes, ERRNO %d", fd, addr, errno);
        FUNC_RET("%d", false);
    }
    if (read(fd, (void *)pword, sizeof(long)) < 0)
    {
        WARN("read");
        FUNC_RET("%d", false);
    }
    close(fd);
#else
#warning "proc_dump() requires procfs"
#endif /* HAVE_PROCFS */
#endif /* DELETED */
    
    FUNC_RET("%d", true);
}

#ifdef HAVE_PROCFS

static bool
check_procfs(pid_t pid)
{
    FUNC_BEGIN("%d", pid);
    assert(pid > 0);
    
    /* Validate procfs */
    struct statfs sb;
    if ((statfs(PROCFS, &sb) < 0) || (sb.f_type != PROC_SUPER_MAGIC))
    {
        FUNC_RET("%d", false);
    }
    
    /* Validate process entries in procfs */
    char buffer[4096];
    sprintf(buffer, PROCFS "/%d",pid);
    if (access(buffer, R_OK | X_OK) < 0)
    {
        /* Patch errno to ESRCH (No such process) */
        if (errno == ENOENT)
        {
            errno = ESRCH;
        }
        FUNC_RET("%d", false);
    }
    
    FUNC_RET("%d", true);
}

#endif /* HAVE_PROCFS */

bool
trace_me(void)
{
    FUNC_BEGIN();
    
    bool res = false;
#ifdef HAVE_PTRACE
    res = (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == 0);
#else
#warning "trace_me() is not implemented for this platform"
#endif /* HAVE_PTRACE */
    
    FUNC_RET("%d", res);
}

bool
trace_next(proc_t * const pproc, trace_type_t type)
{
    FUNC_BEGIN("%p,%d", pproc, type);
    assert(pproc);
    
    long opt = (long)type;
    bool res = (__trace(T_OPTION_NEXT, pproc, NULL, &opt) == 0);
    
    FUNC_RET("%d", res);
}

bool
trace_kill(const proc_t * const pproc, int signo)
{
    FUNC_BEGIN("%p,%d", pproc, signo);
    assert(pproc);
    
    /* Temporary variables */
    pid_t pid = pproc->pid;
    
    /* When the prisoner process is to be killed with SIGKILL, we flush the
     * pending opcode with a sequence of NOP. If there is pending syscall, we
     * also replace the syscall number with SYS_pause. These precautions are
     * essential for certain kernel versions where a killed program has chance
     * to overrun for a few instructions. Otherwise, there is probability that
     * the overrun may lead to undesired side effects or even security risk. */

    /* We only flush opcode when signo is SIGKILL. This is because the prisoner
     * process may continue to execute after handling (or ignoring) a blockable
     * signal. It only makes sense to flush the opcode if the prisoner process
     * is guaranteed to die immediately. */
    
    if (signo == SIGKILL)
    {
        /* Make a local copy of the proc stat buffer */
        proc_t proc = {0};
        memcpy(&proc, pproc, sizeof(proc_t));
        if (!proc_probe(pid, PROBE_REGS | PROBE_OP, &proc))
        {
            WARN("failed to probe process: %d", pid);
            goto skip_rewrite;
        }
        
        /* Flush pending opcode with a sequence of NOP. */
        unsigned long addr = proc.regs.NIP;
        unsigned long nop = 0;
        memset((void *)&nop, OP_NOP, sizeof(unsigned long));
        __trace(T_OPTION_SETDATA, &proc, (void *)addr, (long *)&nop);
        
        /* Replace the pending system call with SYS_pause */
        if (IS_SYSCALL(&proc))
        {
            if (proc.tflags.single_step)
            {
                proc.regs.NAX = SYS_pause;
            }
            else
            {
                proc.regs.ORIG_NAX = SYS_pause;
            }
#ifdef __x86_64__
            proc.regs.cs = 0x33;
#else /* __i386__ */
            proc.regs.xcs = 0x23;
#endif /* __x86_64__ */
            __trace(T_OPTION_SETREGS, &proc, NULL, NULL);
        }
skip_rewrite:
        ;
    }
    
    bool res = (kill(-pid, signo) == 0);
    
    FUNC_RET("%d", res);
}

bool
trace_end(const proc_t * const pproc)
{
    FUNC_BEGIN("%p", pproc);
    assert(pproc);
    
    bool res = (__trace(T_OPTION_END, (proc_t *)pproc, NULL, NULL) == 0);
    
    /* Discard zombie children */
    while (waitpid(-pproc->pid, NULL, WNOHANG) >= 0)
    {
        ;
    }
    
    FUNC_RET("%d", res);
}

/* Most trace_*() functions directly or indirectly invokes ptrace() in linux. 
 * But ptrace() only works when called from the *main* thread initially started 
 * tracing the prisoner process (and thus being the parent of the latter). The 
 * workaround is to make __trace() asynchronous. It places the desired option 
 * (and input) into global variables, and waits for sandbox_tracer() to perform
 * the desired options by calling __trace_impl(), which then calls ptrace().
 * sandbox_tracer() is guaranteed to be running in the *main* thread. */

#define NO_ACTION(act) \
    (((act) == T_OPTION_NOP) || ((act) == T_OPTION_ACK)) \
/* NO_ACTION */

static long
__trace_impl(option_t option, proc_t * const pproc, void * const addr, 
    long * const pdata)
{
    FUNC_BEGIN("%d,%p,%p,%p", option, pproc, addr, pdata);
    assert(!NO_ACTION(option));
    assert(pproc);
    
    if (option == T_OPTION_END)
    {
        FUNC_RET("%ld", 0L);
    }
    
    long res = -1;
    pid_t pid = pproc->pid;
    
    if (pproc->ppid != getpid())
    {
        FUNC_RET("%ld", res);
    }
    
#ifdef HAVE_PTRACE
    switch (option)
    {
    case T_OPTION_NEXT:
        assert(pdata);
        pproc->tflags.single_step = ((int)(*pdata) == TRACE_SINGLE_STEP);
        if (pproc->tflags.single_step)
        {
            res = ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
        }
        else
        {
            res = ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        }
        break;
    case T_OPTION_GETREGS:
        res = ptrace(PTRACE_GETREGS, pid, NULL, (void *)&pproc->regs);
        break;
    case T_OPTION_SETREGS:
        res = ptrace(PTRACE_SETREGS, pid, NULL, (void *)&pproc->regs);
        break;
    case T_OPTION_GETSIGINFO:
        res = ptrace(PTRACE_GETSIGINFO, pid, NULL, (void *)&pproc->siginfo);
        break;
    case T_OPTION_GETDATA:
        assert(addr);
        {
            long temp = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
            res = errno;
            *pdata = (res != 0) ? (*pdata) : (temp);
        }
        break;
    case T_OPTION_SETDATA:
        assert(addr);
        {
            res = ptrace(PTRACE_POKEDATA, pid, addr, *pdata);
        }
        break;
    default:
        res = 0;
        break;
    }
#else
#warning "__trace_impl() is not implemented for this platform"
#endif /* HAVE_PTRACE */
    
    FUNC_RET("%ld", res);
}

/* Global variables for the trace subsystem */

typedef struct
{
    option_t option;
    proc_t * pproc;
    void * addr;
    long data;
    long result;
    int errnum;
} trace_info_t;

static trace_info_t trace_info = {T_OPTION_NOP, NULL, NULL, 0, 0, 0};
static pthread_cond_t trace_update = PTHREAD_COND_INITIALIZER;

typedef struct __pool_item
{
    sandbox_t * psbox;
    SLIST_ENTRY(__pool_item) entries;
} sandbox_mgr_t;

static SLIST_HEAD(__pool_struct, __pool_item) global_pool = \
    SLIST_HEAD_INITIALIZER(global_pool);

typedef struct __pool_struct sandbox_pool_t;

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

static long
__trace(option_t option, proc_t * const pproc, void * const addr, 
    long * const pdata)
{
    FUNC_BEGIN("%d,%p,%p,%p", option, pproc, addr, pdata);
    assert(!NO_ACTION(option));
    assert(pproc);
    
    /* Shortcut to synchronous trace */
    if (pthread_equal(pproc->tflags.trace_id, pthread_self()) && 
        (option != T_OPTION_END))
    {
        FUNC_RET("%ld", __trace_impl(option, pproc, addr, pdata));
    }
    
    long res = 0;
    
    P(&global_mutex);
    
    /* Wait while an existing option is being performed */
    while (trace_info.option != T_OPTION_NOP)
    {
        DBUG("waiting for trace slot");
        pthread_cond_wait(&trace_update, &global_mutex);
    }
    
    DBUG("obtained trace slot");
    
    /* Propose the request */
    trace_info.option = option;
    trace_info.pproc = pproc;
    trace_info.addr = addr;
    trace_info.data = ((pdata != NULL) ? (*pdata) : (0));
    trace_info.result = 0;
    trace_info.errnum = errno;
    pthread_cond_broadcast(&trace_update);
    
    /* Wait while the option is being performed */
    while (trace_info.option != T_OPTION_ACK)
    {
        DBUG("requesting trace option: %s", s_trace_opt_name(trace_info.option));
        pthread_cond_wait(&trace_update, &global_mutex);
    }
    
    if (pdata != NULL)
    {
        *pdata = trace_info.data;
    }
    res = trace_info.result;
    errno = trace_info.errnum;
    
    DBUG("collected trace results");
    
    /* Release slot */
    trace_info.option = T_OPTION_NOP;
    trace_info.pproc = NULL;
    trace_info.addr = NULL;
    trace_info.data = 0;
    trace_info.result = 0;
    trace_info.errnum = errno;
    pthread_cond_broadcast(&trace_update);
    
    DBUG("released trace slot");
    
    V(&global_mutex);
    
    FUNC_RET("%ld", res);
}

void *
sandbox_tracer(void * const dummy)
{
    FUNC_BEGIN("%p", dummy);
    assert(dummy);
    
    sandbox_t * const psbox = (sandbox_t *)dummy;
    
    if (psbox == NULL)
    {
        FUNC_RET("%p", (void *)NULL);
    }
    
    /* Register sandbox to the pool */
    P(&global_mutex);
    sandbox_mgr_t * item = (sandbox_mgr_t *)malloc(sizeof(sandbox_mgr_t));
    item->psbox = psbox;
    SLIST_INSERT_HEAD(&global_pool, item, entries);
    DBUG("registered sandbox %p to the pool", psbox);
    V(&global_mutex);
    
    /* Detect and perform options while the sandbox is running */
    bool end = false;
    
    P(&global_mutex);
    while (!end)
    {
        /* Wait for an option request */
        if (NO_ACTION(trace_info.option))
        {
            DBUG("waiting for new trace option");
            pthread_cond_wait(&trace_update, &global_mutex);
            if (NO_ACTION(trace_info.option))
            {
                continue;
            }
        }
        
        proc_t * pproc = trace_info.pproc;
        if (!pthread_equal(pproc->tflags.trace_id,  pthread_self()))
        {
            continue;
        }
        
        DBUG("received trace option: %s", s_trace_opt_name(trace_info.option));
        
        if (trace_info.option == T_OPTION_END)
        {
            end = true;
        }
        
        /* Notify the trace_*() to collect results */
        trace_info.result = __trace_impl(trace_info.option, trace_info.pproc,
            trace_info.addr, &trace_info.data);
        trace_info.errnum = errno;
        trace_info.option = T_OPTION_ACK;
        pthread_cond_broadcast(&trace_update);
        
        while (trace_info.option == T_OPTION_ACK)
        {
            DBUG("sending trace option: %s", s_trace_opt_name(T_OPTION_ACK));
            pthread_cond_wait(&trace_update, &global_mutex);
        }
        
        DBUG("sent trace option: %s", s_trace_opt_name(T_OPTION_ACK));
    }
    V(&global_mutex);
    
    /* Remove sandbox from the pool */
    P(&global_mutex);
    assert(item);
    SLIST_REMOVE(&global_pool, item, __pool_item, entries);
    free(item);
    DBUG("removed sandbox %p from the pool", psbox);
    V(&global_mutex);
    
    FUNC_RET("%p", (void *)&psbox->result);
}

/**
 * @brief Send signal to monitor threads of a \c sandbox_t object.
 */
static void
sandbox_notify(sandbox_t * const psbox, int signo)
{
    PROC_BEGIN("%p,%d", psbox, signo);
    assert(psbox && (signo > 0));
    
    const pthread_t profiler_thread = psbox->ctrl.monitor[0].tid;
    switch (signo)
    {
    case SIGEXIT:
    case SIGSTAT:
    case SIGPROF:
        pthread_kill(profiler_thread, signo);
        break;
    default:
        /* Wrap the real signal in the payload of SIGEXIT, because the
         * profiler thread of a sandbox instance only waits for the
         * three reserved signals, SIG{EXIT, STAT, PROF}. */
        {
            union sigval sv;
            sv.sival_int = signo;
            if (pthread_sigqueue(profiler_thread, SIGEXIT, sv) != 0)
            {
                WARN("failed pthread_sigqueue()");
            }
        }
        break;
    }
    
    PROC_END();
}

/**
 * @brief Service thread for coordinating active \c sandbox_t objects.
 */
static void *
sandbox_manager(sandbox_pool_t * const pool)
{
    FUNC_BEGIN("%p", pool);
    assert(pool);
    
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGEXIT);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGQUIT);
    sigaddset(&sigmask, SIGINT);
    
    /* The primary task of the manager thread is to send peroidical SIGPROF and
     * SIGSTAT signals to all active sandbox instances. We used to trigger the
     * signals with recurring timers created by timer_create(), and brodcast
     * the signals to all active sandbox instances. However, memory profiling
     * with Massif (http://valgrind.org/docs/manual/ms-manual.html) showed that 
     * timer_create() may cause heap contention in multithreaded scenarios. And 
     * it leads to the creation of new arena consuming 100MB+ memory in its own.
     * Therefore, since 0.3.5-2, we started to use clock_nanosleep() to emulate
     * recurring timers. The problem is to calibrate the sleep time, such that
     * the period of a profiling cycle converges to (1 / PROF_FREQ) sec. Sleep
     * time calibration is implemented as a discrete PID-controller, where SP
     * is the planned profiling cycle (cycle), PV is the measured profiling 
     * cycle (delta), and MV is the calibrated time for sleep (timeout). */
    
    const struct timespec ZERO = {0, 0};
    
    struct timespec cycle = {0, ms2ns(1000 / PROF_FREQ)};
    {
        /* Relax broadcasting frequencies that are too high to realize */
        struct timespec eps = {0, ms2ns(1)};
        if (clock_getres(CLOCK_MONOTONIC, &eps) != 0)
        {
            WARN("failed to get clock resolution");
        }
        TS_INPLACE_ADD(eps, eps);
        if (TS_LESS(cycle, eps))
        {
            cycle = eps;
        }
    }
    DBUG("manager broadcasting at %.2lfHz", 1000. / fts2ms(cycle));
    
    /* Parameters for sleep time calibration */
    const double Kp = 0.75, Ki = 0.25, Kd = 0.0;
    const struct timespec MV_MIN = {0, cycle.tv_nsec / 2};
    const struct timespec MV_MAX = cycle;
    
    DBUG("SP=%.2lf / Kp=%.2lf, Ki=%.2lf, Kd=%.2lf / MV=[%.2lf, %.2lf]", 
        fts2ms(cycle), Kp, Ki, Kd, fts2ms(MV_MIN), fts2ms(MV_MAX));
    
    struct timespec timeout = cycle;
    struct timespec t = ZERO;
    struct timespec prev_error = ZERO;
    struct timespec error = ZERO;
    struct timespec integral = ZERO;
    
    unsigned long count = 0;
    bool end = false;
    
    while (!end)
    {
        /* delta is the measured time of previous profiling cycle */
        struct timespec delta = ZERO;
        if (clock_gettime(CLOCK_MONOTONIC, &delta) != 0)
        {
            WARN("failed to get current time");
            continue;
        }
        else if (TS_LESS(delta, t))
        {
            WARN("invalid previous time");
            continue;
        }
        else
        {
            TS_INPLACE_SUB(delta, t);
            TS_INPLACE_ADD(t, delta);
            if (!TS_LESS(delta, t))
            {
                delta = cycle;
            }
            TS_INPLACE_ADD(error, delta);
            TS_INPLACE_ADD(integral, delta);
        }
        
        siginfo_t siginfo;
        int signo;
        if ((signo = sigtimedwait(&sigmask, &siginfo, &ZERO)) < 0)
        {
            if (errno == EAGAIN)
            {
                siginfo.si_code = SI_TIMER;
                if (count % (PROF_FREQ / STAT_FREQ) == 0)
                {
                    signo = SIGSTAT;
                }
                else
                {
                    signo = SIGPROF;
                }
            }
            else
            {
                WARN("failed to sigtimedwait()");
            }
        }
        
        if (signo >= 0)
        {
            /* SIGEXIT informs the manager thread to quit, and the actual
             * signal to be propagated to sandbox instances is SIGKILL */
            if (signo == SIGEXIT)
            {
                end = true;
                signo = SIGKILL;
            }
            /* Propagate the received signal to all active sandbox instances */
            P(&global_mutex);
            sandbox_mgr_t * item;
            SLIST_FOREACH(item, pool, entries)
            {
                sandbox_notify(item->psbox, signo);
            }
            V(&global_mutex);
        }
        
        /* Calibrate sleep time for this profiling cycle.*/
        if (siginfo.si_code == SI_TIMER)
        {
            ++count;
            TS_INPLACE_SUB(error, cycle);
            TS_INPLACE_SUB(integral, cycle);

            struct timespec derivative = error;
            TS_INPLACE_SUB(derivative, prev_error);
            
            struct timespec feedback = ZERO;
            {
                /* Convert msec feedback to struct timespec equivalent */
                long double fb_msec = Kp * fts2ms(error) + 
                    Ki * fts2ms(integral) + Kd * fts2ms(derivative);
                long double fb_int = 0.0;
                long double fb_frac = modfl(fb_msec / 1000.0, &fb_int);
                feedback = (struct timespec){lrintl(fb_int), 
                    lrintl(ms2ns(fb_frac * 1000.0))}; 
            }
            
            timeout = cycle;
            TS_INPLACE_SUB(timeout, feedback);
            if (TS_LESS(timeout, MV_MIN))
            {
                timeout = MV_MIN;
            }
            if (TS_LESS(MV_MAX, timeout))
            {
                timeout = MV_MAX;
            }
            
            DBUG("manager beacon (%06lu): PV=%.2lf / P=%.2lf, I=%.2lf, D=%.2lf"
                " / MV=%.2lf", count, fts2ms(delta), fts2ms(error), 
                fts2ms(integral), fts2ms(derivative), fts2ms(timeout));
            
            prev_error = error;
            error = ZERO;
            derivative = ZERO;
        }
        else
        {
            /* If the current profiling cycle was interrupted, we subtract the 
             * measured time from timeout and resume sleeping */
            if (TS_LESS(delta, timeout))
            {
                TS_INPLACE_SUB(timeout, delta);
            }
            else
            {
                timeout = ZERO;
            }
        }
        
        /* Sleep for a while according to calibrated time */
        if ((errno = clock_nanosleep(CLOCK_MONOTONIC, 0, &timeout, NULL)) > 0)
        {
            WARN("failed in clock_nanosleep()");
        }
    }
    
    FUNC_RET("%p", (void *)pool);
}

static sigset_t global_oldmask;
static sigset_t global_newmask;
static pthread_t manager_thread;

static __attribute__((constructor)) void 
global_init(void)
{
    PROC_BEGIN();
    
    /* Block signals relevant to libsandbox control, they will be unblocked 
     * and handled by the the manager thread */
    
    sigemptyset(&global_newmask);
    sigaddset(&global_newmask, SIGEXIT);
    sigaddset(&global_newmask, SIGSTAT);
    sigaddset(&global_newmask, SIGPROF);
    sigaddset(&global_newmask, SIGTERM);
    sigaddset(&global_newmask, SIGQUIT);
    sigaddset(&global_newmask, SIGINT);
    
    if (pthread_sigmask(SIG_BLOCK, &global_newmask, &global_oldmask) != 0)
    {
        WARN("pthread_sigmask");
    }
    DBUG("blocked reserved signals");
    
    if (pthread_create(&manager_thread, NULL, (thread_func_t)sandbox_manager, 
        (void *)&global_pool) != 0)
    {
        WARN("failed to create the manager thread at %p", sandbox_manager);
    }
    DBUG("created the manager thread at %p", sandbox_manager);
    
    PROC_END();
}

static __attribute__((destructor)) void
global_fini(void)
{
    PROC_BEGIN();
    
    pthread_kill(manager_thread, SIGEXIT);
    if (pthread_join(manager_thread, NULL) != 0)
    {
        WARN("failed to join the manager thread");
        if (pthread_cancel(manager_thread) != 0)
        {
            WARN("failed to cancel the manager thread");
        }
    }
    DBUG("joined the manager thread");
    
    /* Release references to active sandboxes */
    P(&global_mutex);
    while (!SLIST_EMPTY(&global_pool))
    {
        sandbox_mgr_t * const item = SLIST_FIRST(&global_pool);
        int i;
        for (i = 0; i < (SBOX_MONITOR_MAX); i++)
        {
            pthread_kill(item->psbox->ctrl.monitor[i].tid, SIGEXIT);
        }
        SLIST_REMOVE_HEAD(&global_pool, entries);
        free(item);
    }
    V(&global_mutex);
    
    /* All sandbox instances should have been stopped at this point. Any 
     * pending signal in the reserved set is safe to be ignored. This step is 
     * necessary for non-pool mode, becuase profiling signals are delivered
     * to the entire process and may arrive after the profiler thread of the
     * sandbox instance has been joined. */
    int signo;
    siginfo_t siginfo;
    struct timespec timeout = {0, ms2ns(20)};
    while ((signo = sigtimedwait(&global_newmask, &siginfo, &timeout)) > 0)
    {
        DBUG("flushing signal %d", signo);
    }
    DBUG("flushed all pending signals");
    
    /* Restore saved sigmask */
    if (pthread_sigmask(SIG_SETMASK, &global_oldmask, NULL) != 0)
    {
        WARN("pthread_sigmask");
    }
    DBUG("restored old signal mask");
    
    PROC_END();
}

#ifdef __cplusplus
} /* extern "C" */
#endif
