/*******************************************************************************
 * Copyright (C) 2004-2009, 2011 LIU Yu, pineapple.liu@gmail.com               *
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
    T_KILL = 4, 
    T_GETREGS = 5, 
    T_GETDATA = 6, 
    T_GETSIGINFO = 7, 
    T_SETREGS = 8,
} act_t;

#define NO_ACTION(act) \
    (((act) == T_NOP) || ((act) == T_ACK)) \
/* NO_ACTION */

/* Most trace_*() functions directly or indirectly invokes __trace_act(), which
 * directly calls ptrace() in linux. But ptrace() only works when called from
 * the *main* thread initially started tracing the prisoner process (and thus
 * became the direct parent of the latter). The workaround is when trace_*() is
 * called from another thread, they resort to asynchronous __trace_req() to
 * place the desired action (and input) in a set of global variables, and wait
 * for trace_main() to perform the desired actions by calling __trace_act(). The
 * trace_main() function is guaranteed to be running in the *main* thread. */

typedef long (* trace_proxy_t)(act_t, long * const, proc_t * const);
static long __trace_req(act_t, long * const, proc_t * const);
static long __trace_act(act_t, long * const, proc_t * const);

#define TRACE_PROXY(pproc) \
    RVAL_IF((pproc)->tflags.trace_id == pthread_self()) \
        (__trace_act) \
    RVAL_ELSE \
        (__trace_req) \
    RVAL_FI \
/* TRACE_PROXY */

bool
proc_bind(const void * const dummy, proc_t * const pproc)
{
    FUNC_BEGIN("%p,%p", dummy, pproc);
    assert(dummy && pproc);
    
    if ((dummy == NULL) || (pproc == NULL))
    {
        FUNC_RET("%d", false);
    }
    
    pproc->tflags.trace_id = ((sandbox_t *)dummy)->ctrl.tid;
    
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

    trace_proxy_t __trace = TRACE_PROXY(pproc);
    
    /* Inspect process registers */
    if (opt & PROBE_REGS)
    {
        if (__trace(T_GETREGS, NULL, pproc) != 0)
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
        pproc->op = pproc->regs.rip;
#else /* __i386__ */
        pproc->op = pproc->regs.eip;
#endif /* __x86_64__ */

        if (!pproc->tflags.single_step)
        {
            pproc->op -= 2; // shift back 16bit in syscall tracing mode
        }
        
        if (__trace(T_GETDATA, (long *)&pproc->op, pproc) != 0)
        {
            FUNC_RET("%d", false);
        }
        
        DBG("proc.op             0x%016lx", pproc->op);
    }
    
    /* Inspect signal information */
    if (opt & PROBE_SIGINFO)
    {
        if (__trace(T_GETSIGINFO, NULL, pproc) != 0)
        {
            FUNC_RET("%d", false);
        }

        DBG("proc.siginfo.si_signo       % 10d", pproc->siginfo.si_signo);
        DBG("proc.siginfo.si_errno       % 10d", pproc->siginfo.si_errno);
        DBG("proc.siginfo.si_code        % 10d", pproc->siginfo.si_code);
    }
    
    FUNC_RET("%d", true);
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
    
    trace_proxy_t __trace = TRACE_PROXY(pproc);
    
    long data = (long)addr;
    if (__trace(T_GETDATA, &data, (proc_t *)pproc) != 0)
    {
        FUNC_RET("%d", false);
    }
    
    *pword = data;
    
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

    DBG("data.%p     0x%016lx", addr, *pword);

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
    
    trace_proxy_t __trace = TRACE_PROXY(pproc);
    
    bool res = (__trace(T_SETREGS, NULL, pproc) == 0);
    
    FUNC_RET("%d", res);
}

bool
trace_next(proc_t * const pproc, trace_type_t type)
{
    FUNC_BEGIN("%p,%d", pproc, type);
    assert(pproc);
    
    trace_proxy_t __trace = TRACE_PROXY(pproc);
    
    long data = (long)type;
    bool res = (__trace(T_NEXT, &data, pproc) == 0);
    
    FUNC_RET("%d", res);
}

bool
trace_kill(proc_t * const pproc, int signo)
{
    FUNC_BEGIN("%p,%d", pproc, signo);
    assert(pproc);
    
    trace_proxy_t __trace = TRACE_PROXY(pproc);
    
    long data = (long)(signo);
    bool res = (__trace(T_KILL, &data, (proc_t *)pproc) == 0);
    
    FUNC_RET("%d", res);
}

bool
trace_end(const proc_t * const pproc)
{
    FUNC_BEGIN("%p", pproc);
    assert(pproc);
    
    trace_proxy_t __trace = TRACE_PROXY(pproc);
    bool res = (__trace(T_END, NULL, (proc_t *)pproc) == 0);
    
    FUNC_RET("%d", res);
}

static long
__trace_act(act_t action, long * const pdata, proc_t * const pproc)
{
    FUNC_BEGIN("%d,%p,%p", action, pdata, pproc);
    assert(!NO_ACTION(action));
    assert(pproc);
    assert(pproc->tflags.trace_id == pthread_self());
    
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
    case T_KILL:
        assert(pdata);
        if (pproc->tflags.is_in_syscall)
        {
            if (pproc->tflags.single_step)
            {
                ptrace(PTRACE_POKEUSER, pid, NAX * sizeof(long), SYS_pause);
            }
            else
            {
                ptrace(PTRACE_POKEUSER, pid, ORIG_NAX * sizeof(long), SYS_pause);
            }
        }
        res = kill(-pid, (int)(*pdata));
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
        assert(pdata);
        *pdata = ptrace(PTRACE_PEEKDATA, pid, (void *)(*pdata), NULL);
        res = errno;
        break;
    default:
        res = 0;
        break;
    }
#else
#warning "__trace_act() is not implemented for this platform"
#endif /* HAVE_PTRACE */
    
    FUNC_RET("%ld", res);
}

typedef struct
{
    act_t action;
    proc_t * pproc;
    long data;
    long result;
} trace_info_t;

static pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t trace_notice = PTHREAD_COND_INITIALIZER;
static trace_info_t trace_info = {T_NOP, NULL, 0, 0};

static long
__trace_req(act_t action, long * const pdata, proc_t * const pproc)
{
    FUNC_BEGIN("%d,%p,%p", action, pdata, pproc);
    assert(!NO_ACTION(action));
    assert(pproc);
    
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
        FUNC_RET("%p", NULL);
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
        
        trace_info.result = __trace_act(trace_info.action, &trace_info.data,
            trace_info.pproc);
        
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
