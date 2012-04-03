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

#include "platform.h"
#include "sandbox.h"
#include "symbols.h"
#include <assert.h>             /* assert() */
#include <ctype.h>              /* toupper() */
#include <fcntl.h>              /* open(), close(), O_RDONLY */
#include <stdio.h>              /* read(), sscanf(), sprintf() */
#include <string.h>             /* memset(), strsep() */
#include <sys/types.h>          /* off_t */
#include <sys/times.h>          /* struct tms, struct timeval */
#include <unistd.h>             /* access(), lseek(), {R,F}_OK */

#include "config.h"

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
    T_NOP = 0, 
    T_ACK = 1, 
    T_END = 2, 
    T_NEXT = 3, 
    T_GETREGS = 4, 
    T_GETDATA = 5, 
    T_GETSIGINFO = 6, 
    T_SETREGS = 7,
    T_SETDATA = 8,
} act_t;

static long __trace(act_t, proc_t * const, void * const, long * const);

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
    pproc->tflags.trace_id = psbox->ctrl.tid;
    
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
        WARNING("procfs entries missing or invalid");
        FUNC_RET("%d", false);
    }
    
    char buffer[4096];    
    sprintf(buffer, PROCFS "/%d/stat", pid);
   
    int fd = open(buffer, O_RDONLY);
    if (fd < 0)
    {
        WARNING("procfs stat missing or unaccessable");
        FUNC_RET("%d", false);
    }
    
    int len = read(fd, buffer, sizeof(buffer) - 1);
    
    close(fd);

    if (len < 0)
    {
        WARNING("failed to grab stat from procfs");
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
            sscanf(token, "%lu", &pproc->cminflt);
            break;
        case 11:                /* maj_flt */
            sscanf(token, "%lu", &pproc->majflt);
            break;
        case 12:                /* cmaj_flt */
            sscanf(token, "%lu", &pproc->cmajflt);
            break;
        case 13:                /* utime */
            sscanf(token, "%lu", &tm.tms_utime);
            break;
        case 14:                /* stime */
            sscanf(token, "%lu", &tm.tms_stime);
            break;
        case 15:                /* cutime */
            sscanf(token, "%ld", &tm.tms_cutime);
            break;
        case 16:                /* cstime */
            sscanf(token, "%ld", &tm.tms_cstime);
            break;
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
    TS_UPDATE_CLK(&pproc->cutime, tm.tms_cutime);
    TS_UPDATE_CLK(&pproc->cstime, tm.tms_cstime);
    
    DBG("proc.pid                    % 10d", pproc->pid);
    DBG("proc.ppid                   % 10d", pproc->ppid);
    DBG("proc.state                           %c", pproc->state);
    DBG("proc.flags          0x%016lx", pproc->flags);
    DBG("proc.utime                  %010lu", ts2ms(pproc->utime));
    DBG("proc.stime                  %010lu", ts2ms(pproc->stime));
    DBG("proc.cutime                 % 10ld", ts2ms(pproc->cutime));
    DBG("proc.cstime                 % 10ld", ts2ms(pproc->cstime));
    DBG("proc.minflt                 %010lu", pproc->minflt);
    DBG("proc.cminflt                %010lu", pproc->cminflt);
    DBG("proc.majflt                 %010lu", pproc->majflt);
    DBG("proc.cmajflt                %010lu", pproc->cmajflt);
    DBG("proc.vsize                  %010lu", pproc->vsize);
    DBG("proc.rss                    % 10ld", pproc->rss);
    
#else
#warning "proc_probe() requires procfs"
#endif /* HAVE_PROCFS */
    
    /* Inspect process registers */
    if (opt & PROBE_REGS)
    {
        if (__trace(T_GETREGS, pproc, NULL, NULL) != 0)
        {
            FUNC_RET("%d", false);
        }
#ifdef __x86_64__
        DBG("regs.r15            0x%016lx", pproc->regs.r15);
        DBG("regs.r14            0x%016lx", pproc->regs.r14);
        DBG("regs.r13            0x%016lx", pproc->regs.r13);
        DBG("regs.r12            0x%016lx", pproc->regs.r12);
        DBG("regs.rbp            0x%016lx", pproc->regs.rbp);
        DBG("regs.rbx            0x%016lx", pproc->regs.rbx);
        DBG("regs.r11            0x%016lx", pproc->regs.r11);
        DBG("regs.r10            0x%016lx", pproc->regs.r10);
        DBG("regs.r9             0x%016lx", pproc->regs.r9);
        DBG("regs.r8             0x%016lx", pproc->regs.r8);
        DBG("regs.rax            0x%016lx", pproc->regs.rax);
        DBG("regs.rcx            0x%016lx", pproc->regs.rcx);
        DBG("regs.rdx            0x%016lx", pproc->regs.rdx);
        DBG("regs.rsi            0x%016lx", pproc->regs.rsi);
        DBG("regs.rdi            0x%016lx", pproc->regs.rdi);
        DBG("regs.orig_rax       0x%016lx", pproc->regs.orig_rax);
        DBG("regs.rip            0x%016lx", pproc->regs.rip);
        DBG("regs.cs             0x%016lx", pproc->regs.cs);
        DBG("regs.eflags         0x%016lx", pproc->regs.eflags);
        DBG("regs.rsp            0x%016lx", pproc->regs.rsp);
        DBG("regs.ss             0x%016lx", pproc->regs.ss);
        DBG("regs.fs_base        0x%016lx", pproc->regs.fs_base);
        DBG("regs.gs_base        0x%016lx", pproc->regs.gs_base);
        DBG("regs.ds             0x%016lx", pproc->regs.ds);
        DBG("regs.es             0x%016lx", pproc->regs.es);
        DBG("regs.fs             0x%016lx", pproc->regs.fs);
        DBG("regs.gs             0x%016lx", pproc->regs.gs);
#else /* __i386__ */
        DBG("regs.ebx            0x%016lx", pproc->regs.ebx);
        DBG("regs.ecx            0x%016lx", pproc->regs.ecx);
        DBG("regs.edx            0x%016lx", pproc->regs.edx);
        DBG("regs.esi            0x%016lx", pproc->regs.esi);
        DBG("regs.edi            0x%016lx", pproc->regs.edi);
        DBG("regs.ebp            0x%016lx", pproc->regs.ebp);
        DBG("regs.eax            0x%016lx", pproc->regs.eax);
        DBG("regs.xds            0x%016lx", pproc->regs.xds);
        DBG("regs.xes            0x%016lx", pproc->regs.xes);
        DBG("regs.xfs            0x%016lx", pproc->regs.xfs);
        DBG("regs.xgs            0x%016lx", pproc->regs.xgs);
        DBG("regs.orig_eax       0x%016lx", pproc->regs.orig_eax);
        DBG("regs.eip            0x%016lx", pproc->regs.eip);
        DBG("regs.xcs            0x%016lx", pproc->regs.xcs);
        DBG("regs.eflags         0x%016lx", pproc->regs.eflags);
        DBG("regs.esp            0x%016lx", pproc->regs.esp);
        DBG("regs.xss            0x%016lx", pproc->regs.xss);
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
        
        if (__trace(T_GETDATA, pproc, (void *)addr, (long *)&pproc->op) != 0)
        {
            FUNC_RET("%d", false);
        }
        
        DBG("proc.op             0x%016lx", pproc->op);
    }
    
    /* Inspect signal information */
    if (opt & PROBE_SIGINFO)
    {
        if (__trace(T_GETSIGINFO, pproc, NULL, NULL) != 0)
        {
            FUNC_RET("%d", false);
        }

        DBG("proc.siginfo.si_signo       % 10d", pproc->siginfo.si_signo);
        DBG("proc.siginfo.si_errno       % 10d", pproc->siginfo.si_errno);
        DBG("proc.siginfo.si_code        % 10d", pproc->siginfo.si_code);
    }
    
    FUNC_RET("%d", true);
}

int
syscall_mode(proc_t * const pproc)
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
    if (__trace(T_GETDATA, pproc, addr, (long *)&code) != 0)
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
            if (__trace(T_GETDATA, pproc, addr, (long *)&pproc->op) != 0)
            {
                FUNC_RET("%d", SCMODE_MAX);
            }
            DBG("vsyscall_addr       0x%016lx", addr);
            DBG("vsyscall_code       0x%016lx", pproc->op);
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
    
    if (__trace(T_GETDATA, (proc_t *)pproc, (void *)addr, pword) != 0)
    {
        FUNC_RET("%d", false);
    }    
    
    DBG("data.%p     0x%016lx", addr, *pword);
    
#ifdef DELETED
#ifdef HAVE_PROCFS
    /* Access the memory of targeted process via procfs */
    char buffer[4096];

    sprintf(buffer, PROCFS "/%d/mem", pproc->pid);
    if (access(buffer, R_OK | F_OK) < 0)
    {
        WARNING("procfs entries missing or invalid");
        FUNC_RET("%d", false);
    }

    /* Copy a word from targeted address */
    int fd = open(buffer, O_RDONLY);
    if (lseek(fd, (off_t)addr, SEEK_SET) < 0)
    {
        extern int errno;
        WARNING("lseek(%d, %p, SEEK_SET) failes, ERRNO %d", fd, addr, errno);
        FUNC_RET("%d", false);
    }
    if (read(fd, (void *)pword, sizeof(unsigned long)) < 0)
    {
        WARNING("read");
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
trace_self(void)
{
    FUNC_BEGIN();
    
    bool res = false;
#ifdef HAVE_PTRACE
    res = (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == 0);
#else
#warning "trace_self() is not implemented for this platform"
#endif /* HAVE_PTRACE */
    
    FUNC_RET("%d", res);
}

bool
trace_hack(proc_t * const pproc)
{
    FUNC_BEGIN("%p", pproc);
    assert(pproc);
    
#ifdef DELETED
    bool res = (__trace(T_SETREGS, pproc, NULL, NULL) == 0);
    
    FUNC_RET("%d", res);
#else    
    FUNC_RET("%d", true);
#endif /* DELETED */
}

bool
trace_next(proc_t * const pproc, trace_type_t type)
{
    FUNC_BEGIN("%p,%d", pproc, type);
    assert(pproc);
    
    long opt = (long)type;
    bool res = (__trace(T_NEXT, pproc, NULL, &opt) == 0);
    
    FUNC_RET("%d", res);
}

bool
trace_kill(proc_t * const pproc, int signo)
{
    FUNC_BEGIN("%p,%d", pproc, signo);
    assert(pproc);
    
    /* We only do opcode rewrite when signo is non-blockable, i.e. SIGKILL or 
     * SIGSTOP. This is because the prisoner process may continue to execute
     * after handling (or ignoring) a blockable signal. It is only reasonable to
     * rewrite the opcode to a sequence of nop's, if the prisoner process is 
     * guaranteed to terminate or get killed immediately. */
    
    if ((signo == SIGKILL) || (signo == SIGSTOP))
    {
        if (proc_probe(pproc->pid, PROBE_REGS, pproc))
        {
#ifdef __x86_64__
            unsigned char * addr = (unsigned char *)pproc->regs.rip;
            unsigned long nop = 0x9090909090909090UL;
#else /* __i386__ */
            unsigned char * addr = (unsigned char *)pproc->regs.eip;
            unsigned long nop = 0x90909090UL;
#endif /* __x86_64__ */        
            __trace(T_SETDATA, pproc, (void *)addr, (long *)&nop);
        }
    }
    
    bool res = (kill(-(pproc->pid), signo) == 0);
    
#ifdef DELETED   
    if (pproc->tflags.is_in_syscall)
    {
        long scno = SYS_pause;
#ifdef __x86_64__
        if (pproc->syscall_mode == SCMODE_LINUX32)
        {
            scno = SYS32_pause;
        }
#endif /* __x86_64__ */
        if (pproc->tflags.single_step)
        {
            pproc->regs.NAX = scno;
        }
        else
        {
            pproc->regs.ORIG_NAX = scno;
        }
        __trace(T_SETREGS, (proc_t *)pproc, NULL, NULL);
    }
#endif /* DELETED */
    
    FUNC_RET("%d", res);
}

bool
trace_end(const proc_t * const pproc)
{
    FUNC_BEGIN("%p", pproc);
    assert(pproc);
    
    bool res = (__trace(T_END, (proc_t *)pproc, NULL, NULL) == 0);
    
    FUNC_RET("%d", res);
}

/* Most trace_*() functions directly or indirectly invokes ptrace() in linux. 
 * But ptrace() only works when called from the *main* thread initially started 
 * tracing the prisoner process (and thus being the parent of the latter). The 
 * workaround is to make __trace() asynchronous. It places the desired action 
 * (and input) into a set of global variables, and waits for trace_main() to 
 * perform the desired actions by calling __trace_impl(), which then calls 
 * ptrace(). trace_main() is guaranteed to be running in the *main* thread. */

#define NO_ACTION(act) \
    (((act) == T_NOP) || ((act) == T_ACK)) \
/* NO_ACTION */

static long
__trace_impl(act_t action, proc_t * const pproc, void * const addr, 
    long * const pdata)
{
    FUNC_BEGIN("%d,%p,%p,%p", action, pproc, addr, pdata);
    assert(!NO_ACTION(action));
    assert(pproc);
    
    if (action == T_END)
    {
        FUNC_RET("%ld", 0L);
    }
    
    long res = -1;
    pid_t pid = pproc->pid;
    
    if ((pproc->ppid != getpid()) || (toupper(pproc->state) != 'T'))
    {
        FUNC_RET("%ld", res);
    }
    
#ifdef HAVE_PTRACE
    switch (action)
    {
    case T_NEXT:
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
    case T_GETREGS:
        res = ptrace(PTRACE_GETREGS, pid, NULL, (void *)&pproc->regs);
        break;
    case T_SETREGS:
        res = ptrace(PTRACE_SETREGS, pid, NULL, (void *)&pproc->regs);
        break;
    case T_GETSIGINFO:
        res = ptrace(PTRACE_GETSIGINFO, pid, NULL, (void *)&pproc->siginfo);
        break;
    case T_GETDATA:
        assert(addr);
        {
            long temp = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
            res = errno;
            *pdata = (res != 0)?(*pdata):(temp);
        }
        break;
    case T_SETDATA:
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

typedef struct
{
    act_t action;
    proc_t * pproc;
    void * addr;
    long data;
    long result;
} trace_info_t;

static pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t trace_notice = PTHREAD_COND_INITIALIZER;
static trace_info_t trace_info = {T_NOP, NULL, NULL, 0, 0};

static long
__trace(act_t action, proc_t * const pproc, void * const addr, 
    long * const pdata)
{
    FUNC_BEGIN("%d,%p,%p,%p", action, pproc, addr, pdata);
    assert(!NO_ACTION(action));
    assert(pproc);
    
    /* Shortcut to synchronous trace */
    if ((pproc->tflags.trace_id == pthread_self()) && (action != T_END))
    {
        FUNC_RET("%ld", __trace_impl(action, pproc, addr, pdata));
    }
    
    long res = 0;
    
    P(&trace_mutex);
    
    /* Wait while an existing action is being performed */
    while (trace_info.action != T_NOP)
    {
        DBG("waiting for slot");
        pthread_cond_wait(&trace_notice, &trace_mutex);
    }
    
    DBG("obtained slot");
    
    /* Propose the request */
    trace_info.action = action;
    trace_info.pproc = pproc;
    trace_info.addr = addr;
    trace_info.data = ((pdata != NULL)?(*pdata):(0));
    trace_info.result = 0;
    
    /* Wait while the action is being performed */
    while (trace_info.action != T_ACK)
    {
        DBG("requesting action %s", trace_act_name(trace_info.action));
        pthread_cond_broadcast(&trace_notice);
        pthread_cond_wait(&trace_notice, &trace_mutex);
    }
    
    if (pdata != NULL)
    {
        *pdata = trace_info.data;
    }
    res = trace_info.result;
    
    DBG("collected results");
    
    /* Release slot */
    trace_info.action = T_NOP;
    trace_info.pproc = NULL;
    trace_info.addr = NULL;
    trace_info.data = 0;
    trace_info.result = 0;
    pthread_cond_broadcast(&trace_notice);
    
    DBG("released slot");
    
    V(&trace_mutex);
    
    FUNC_RET("%ld", res);
}

void *
trace_main(void * const dummy)
{
    FUNC_BEGIN("%p", dummy);
    assert(dummy);
    
    sandbox_t * psbox = (sandbox_t *)dummy;
    
    if (psbox == NULL)
    {
        FUNC_RET("%p", (void *)NULL);
    }
    
    /* Detect and perform actions while the sandbox is running */
    bool end = false;
    
    P(&trace_mutex);
    while (!end)
    {
        /* Wait for an action request */
        if (NO_ACTION(trace_info.action))
        {
            DBG("waiting for new action");
            pthread_cond_wait(&trace_notice, &trace_mutex);
            if (NO_ACTION(trace_info.action))
            {
                continue;
            }
        }
        
        proc_t * pproc = trace_info.pproc;
        if (pproc->tflags.trace_id != pthread_self())
        {
            continue;
        }
        
        DBG("received %s", trace_act_name(trace_info.action));
        
        if (trace_info.action == T_END)
        {
            end = true;
        }
        
        trace_info.result = __trace_impl(trace_info.action, trace_info.pproc,
            trace_info.addr, &trace_info.data);
        
        /* Notify the trace_*() to collect results */
        trace_info.action = T_ACK;
        
        while (trace_info.action == T_ACK)
        {
            DBG("sending %s", trace_act_name(T_ACK));
            pthread_cond_broadcast(&trace_notice);
            pthread_cond_wait(&trace_notice, &trace_mutex);
        }
        
        DBG("sent %s", trace_act_name(T_ACK));
    }
    V(&trace_mutex);
    
    FUNC_RET("%p", (void *)&psbox->result);
}

#ifndef CACHE_SIZE
#define CACHE_SIZE (1 << 21)    /* assume 2MB cache */
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
        FUNC_RET("%d", false);
    }
    
    FUNC_RET("%d", true);
}

#endif /* HAVE_PROCFS */

#ifdef __cplusplus
} /* extern "C" */
#endif
