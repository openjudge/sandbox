/*******************************************************************************
 * Copyright (C) 2004-2009, 2011, 2012 LIU Yu, pineapple.liu@gmail.com         *
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
#include <pthread.h>            /* pthread_{create,join,...}() */
#include <signal.h>             /* kill(), SIG* */
#include <stdio.h>              /* read(), sscanf(), sprintf() */
#include <stdlib.h>             /* malloc(), free() */
#include <string.h>             /* memset(), strsep() */
#include <sys/queue.h>          /* SLIST_*() */
#include <sys/syscall.h>        /* syscall(), SYS_gettid */
#include <sys/types.h>          /* off_t */
#include <sys/times.h>          /* struct tms, struct timeval */
#include <unistd.h>             /* access(), lseek(), {R,F}_OK */

#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id _sigev_un._tid
#endif /* sigev_notify_thread_id */

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
    
    close(fd);

    if (len < 0)
    {
        WARN("failed to grab stat from procfs");
        FUNC_RET("%d", false);
    }

    buffer[len] = '\0';
    
    /* Extract interested information */
    struct tms tm;
    int offset = 0;
    char * token = buffer;
    do
    {
        switch (offset++)
        {
        case  0:                /* pid */
            sscanf(token, "%d", &pproc->pid);
            break;
        case  1:                /* comm */
            break;
        case  2:                /* state */
            sscanf(token, "%c", &pproc->state);
            break;
        case  3:                /* ppid */
            sscanf(token, "%d", &pproc->ppid);
            break;
        case  4:                /* pgrp */
        case  5:                /* session */
        case  6:                /* tty_nr */
        case  7:                /* tty_pgrp */
            break;
        case  8:                /* flags */
            sscanf(token, "%lu", &pproc->flags);
            break;
        case  9:                /* min_flt */
            sscanf(token, "%lu", &pproc->minflt);
            break;
        case 10:                /* cmin_flt */
            break;
        case 11:                /* maj_flt */
            sscanf(token, "%lu", &pproc->majflt);
            break;
        case 12:                /* cmaj_flt */
            break;
        case 13:                /* utime */
            sscanf(token, "%lu", &tm.tms_utime);
            break;
        case 14:                /* stime */
            sscanf(token, "%lu", &tm.tms_stime);
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
            sscanf(token, "%lu", &pproc->vsize);
            break;
        case 23:                /* rss */
            sscanf(token, "%ld", &pproc->rss);
            break;
        case 24:                /* rlim_rss */
        case 25:                /* start_code */
            sscanf(token, "%lu", &pproc->start_code);
            break;
        case 26:                /* end_code */
            sscanf(token, "%lu", &pproc->end_code);
            break;
        case 27:                /* start_stack */
            sscanf(token, "%lu", &pproc->start_stack);
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
#ifdef __x86_64__
        unsigned char * addr = (unsigned char *)pproc->regs.rip;
#else /* __i386__ */
        unsigned char * addr = (unsigned char *)pproc->regs.eip;
#endif /* __x86_64__ */

        if (!pproc->tflags.single_step)
        {
            addr -= 2; /* backoff 2 bytes in system call tracing mode */
        }
        
        if (__trace(T_OPTION_GETDATA, pproc, (void *)addr, 
            (long *)&pproc->op) != 0)
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
proc_syscall_mode(proc_t * const pproc)
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
     * I observed this issue when running static ELF-i386 programs on 
     * kernel-2.6.32-220.7.1.el6.x86_64. Perhaps a bug in the way the Linux 
     * kernel handles ptrace(). As a workaround, we can follow the JMP for 
     * a few steps to locate the actual instruction. In its current status,
     * we have only implemented JMP REL (i.e. EB and E9) inspection. Other 
     * types of JMP / CALL instructions are treated as illegal scmode. */
    
    unsigned char * addr = (unsigned char *)pproc->regs.rip;
    
    switch (OPCODE16(pproc->op) & 0xffUL)
    {
    case 0xeb: /* jmp rel short */
        addr += (char)((pproc->op & 0xff00UL) >> 8);
        break;
    case 0xe9: /* jmp rel */
        addr += (int)((pproc->op & 0xffffffff00UL) >> 8);
        break;
    case 0xea: /* jmp far */
    case 0xff: /* call */
    default:
        FUNC_RET("%d", SYSCALL_MODE(pproc));
        break;
    }
    
    unsigned long code;
    if (__trace(T_OPTION_GETDATA, pproc, addr, (long *)&code) != 0)
    {
        FUNC_RET("%d", SCMODE_MAX);
    }
    
    size_t offset;
    for (offset = 0; offset < sizeof(long) - 1; offset++)
    {
        if ((OPCODE16(code) == OP_INT80) || \
            (OPCODE16(code) == OP_SYSENTER) || \
            (OPCODE16(code) == OP_SYSCALL))
        {
            addr += offset;
            if (__trace(T_OPTION_GETDATA, pproc, addr, (long *)&pproc->op) != 0)
            {
                FUNC_RET("%d", SCMODE_MAX);
            }
            DBUG("vsyscall_addr       0x%016lx", (unsigned long)addr);
            DBUG("vsyscall_code       0x%016lx", pproc->op);
            break;
        }
        code = (code >> 8);
    }
    
#endif /* __x86_64__ */
#endif /* WITH_VSYSCALL_INSPECT */
    
    FUNC_RET("%d", SYSCALL_MODE(pproc));
}

bool
proc_dump(const proc_t * const pproc, const void * const addr, 
          long * const pword)
{
    FUNC_BEGIN("%p,%p,%p", pproc, addr, pword);
    assert(pproc && addr && pword);
    
    if ((pproc == NULL) || (addr == NULL) || (pword == NULL))
    {
        FUNC_RET("%d", false);
    }
    
    if (__trace(T_OPTION_GETDATA, (proc_t *)pproc, (void *)addr, pword) != 0)
    {
        FUNC_RET("%d", false);
    }    
    
    DBUG("data.%p     0x%016lx", addr, *pword);
    
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
    if (read(fd, (void *)pword, sizeof(unsigned long)) < 0)
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
trace_kill(proc_t * const pproc, int signo)
{
    FUNC_BEGIN("%p,%d", pproc, signo);
    assert(pproc);
    
    bool res = (kill(-(pproc->pid), signo) == 0);
    
    FUNC_RET("%d", res);
}

bool
trace_end(const proc_t * const pproc)
{
    FUNC_BEGIN("%p", pproc);
    assert(pproc);
    
    bool res = (__trace(T_OPTION_END, (proc_t *)pproc, NULL, NULL) == 0);
    
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
            *pdata = (res != 0)?(*pdata):(temp);
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
    trace_info.data = ((pdata != NULL)?(*pdata):(0));
    trace_info.result = 0;
    trace_info.errnum = errno;
    pthread_cond_broadcast(&trace_update);
    
    /* Wait while the option is being performed */
    while (trace_info.option != T_OPTION_ACK)
    {
        DBUG("requesting trace option: %s", t_option_name(trace_info.option));
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
    
#ifdef WITH_TRACE_POOL
    /* Register sandbox to the pool */
    P(&global_mutex);
    sandbox_ref_t * item = (sandbox_ref_t *)malloc(sizeof(sandbox_ref_t));
    item->psbox = psbox;
    SLIST_INSERT_HEAD(&sandbox_pool, item, entries);
    DBUG("registered sandbox %p to the pool", psbox);
    V(&global_mutex);
#endif /* WITH_TRACE_POOL */
    
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
        
        DBUG("received trace option: %s", t_option_name(trace_info.option));
        
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
            DBUG("sending trace option: %s", t_option_name(T_OPTION_ACK));
            pthread_cond_wait(&trace_update, &global_mutex);
        }
        
        DBUG("sent trace option: %s", t_option_name(T_OPTION_ACK));
    }
    V(&global_mutex);
    
#ifdef WITH_TRACE_POOL
    /* Remove sandbox from the pool */
    P(&global_mutex);
    assert(item);
    SLIST_REMOVE(&sandbox_pool, item, __pool_item, entries);
    free(item);
    DBUG("removed sandbox %p from the pool", psbox);
    V(&global_mutex);
#endif /* WITH_TRACE_POOL */
    
    FUNC_RET("%p", (void *)&psbox->result);
}

#ifndef CACHE_SIZE
#define CACHE_SIZE (1 << 22)    /* assume 4MB cache */
#endif /* CACHE_SIZE */

static int cache[CACHE_SIZE / sizeof(int)];
volatile int cache_sink;        /* variable used for clearing cache */

void 
cache_flush(void)
{
    PROC_BEGIN();
    unsigned int i;
    unsigned int sum = 0;
    for (i = 0; i < CACHE_SIZE / sizeof(int); i++) cache[i] = 3;
    for (i = 0; i < CACHE_SIZE / sizeof(int); i++) sum += cache[i];
    cache_sink = sum;
    PROC_END();
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

timer_t
sandbox_timer(int signo, int freq)
{
    FUNC_BEGIN("%d,%d", signo, freq);
    assert((signo > 0) && (freq > 0));
    
    timer_t timer;
    
    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_value.sival_ptr = &timer;
    sev.sigev_signo = signo;
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_notify_thread_id = syscall(SYS_gettid);
    
    if (timer_create(CLOCK_MONOTONIC, &sev, &timer) != 0)
    {
        FUNC_RET(timer);
    }
    
    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = ms2ns(1000 / freq);
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = ms2ns(20); // warmup time
    
    if (timer_settime(timer, 0, &its, NULL) != 0)
    {
        FUNC_RET(timer);
    }
    
    FUNC_RET(timer);
}

void
sandbox_notify(sandbox_t * const psbox, int signo)
{
    PROC_BEGIN("%p", psbox);
    assert(psbox && (signo > 0));
    
    const pthread_t profiler = psbox->ctrl.monitor[0].tid;
    switch (signo)
    {
    case SIGEXIT:
    case SIGSTAT:
    case SIGPROF:
        pthread_kill(profiler, signo);
        break;
    default:
        /* Wrap the real signal in the payload of SIGEXIT, because the
         * profiler thread of a sandbox instance only waits for the
         * three reserved signals, SIG{EXIT, STAT, PROF}. */
        {
            union sigval sv;
            sv.sival_int = signo;
            if (pthread_sigqueue(profiler, SIGEXIT, sv) != 0)
            {
                WARN("failed pthread_sigqueue()");
            }
        }
        break;
    }
    
    PROC_END();
}

static sigset_t sigmask;
static sigset_t oldmask;

#ifdef WITH_TRACE_POOL

static pthread_t manager;

#endif /* WITH_TRACE_POOL */

static __attribute__((constructor)) void 
init(void)
{
    PROC_BEGIN();
    
    /* Block signals relevant to libsandbox control, they will be unblocked 
     * and handled by the the manager thread (pool mode) or by the profiler 
     * threads of individual sandboxes (non-pool mode). */
    
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGEXIT);
    sigaddset(&sigmask, SIGSTAT);
    sigaddset(&sigmask, SIGPROF);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGQUIT);
    sigaddset(&sigmask, SIGINT);
    
    if (pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask) != 0)
    {
        WARN("pthread_sigmask");
    }
    DBUG("blocked reserved signals");
    
#ifdef WITH_TRACE_POOL
    
    if (pthread_create(&manager, NULL, (thread_func_t)sandbox_manager, 
        (void *)&sandbox_pool) != 0)
    {
        WARN("failed to create the manager thread at %p", sandbox_manager);
    }
    DBUG("created the manager thread at %p", sandbox_manager);
    
#endif /* WITH_TRACE_POOL */
    
    PROC_END();
}

static __attribute__((destructor)) void
fini(void)
{
    PROC_BEGIN();
    
#ifdef WITH_TRACE_POOL

    pthread_kill(manager, SIGEXIT);
    if (pthread_join(manager, NULL) != 0)
    {
        WARN("failed to join the manager thread");
        if (pthread_cancel(manager) != 0)
        {
            WARN("failed to cancel the manager thread");
        }
    }
    DBUG("joined the manager thread");
    
    /* Release references to active sandboxes */
    P(&global_mutex);
    while (!SLIST_EMPTY(&sandbox_pool))
    {
        sandbox_ref_t * const item = SLIST_FIRST(&sandbox_pool);
        int i;
        for (i = 0; i < (SBOX_MONITOR_MAX); i++)
        {
            pthread_kill(item->psbox->ctrl.monitor[i].tid, SIGEXIT);
        }
        SLIST_REMOVE_HEAD(&sandbox_pool, entries);
        free(item);
    }
    V(&global_mutex);
    
#endif /* WITH_TRACE_POOL */
    
    /* All sandbox instances should have been stopped at this point. Any 
     * pending signal in the reserved set is safe to be ignored. This step is 
     * necessary for non-pool mode, becuase profiling signals are delivered
     * to the entire process and may arrive after the profiler thread of the
     * sandbox instance has been joined. */
    int signo;
    siginfo_t siginfo;
    struct timespec timeout = {0, ms2ns(20)};
    while ((signo = sigtimedwait(&sigmask, &siginfo, &timeout)) > 0)
    {
        DBUG("flushing signal %d", signo);
    }
    DBUG("flushed all pending signals");
    
    /* Restore saved sigmask */
    if (pthread_sigmask(SIG_SETMASK, &oldmask, NULL) != 0)
    {
        WARN("pthread_sigmask");
    }
    DBUG("restored old signal mask");
    
    PROC_END();
}

#ifdef __cplusplus
} /* extern "C" */
#endif
