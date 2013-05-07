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

#include "sandbox.h"
#include "platform.h"
#include "internal.h" 
#include "config.h"

#include <errno.h>              /* ECHILD, EINVAL */
#include <grp.h>                /* struct group, getgrgid() */
#include <pwd.h>                /* struct passwd, getpwuid() */
#include <pthread.h>            /* pthread_{create,join,sigmask,...}() */
#include <signal.h>             /* kill(), SIG* */
#include <stdlib.h>             /* EXIT_{SUCCESS,FAILURE} */
#include <string.h>             /* str{cpy,cmp,str}(), mem{set,cpy}() */
#include <sys/stat.h>           /* struct stat, stat(), fstat() */
#include <sys/resource.h>       /* getrlimit(), setrlimit() */
#include <sys/wait.h>           /* waitid(), P_* */
#include <time.h>               /* clock_get{cpuclockid,time}(), ... */
#include <unistd.h>             /* fork(), access(), chroot(), getpid(),
                                   getpagesize(), {R,X}_OK,
                                   STD{IN,OUT,ERR}_FILENO */

#ifdef DELETED
#include <sys/time.h>           /* setitimer(), ITIMER_PROF */
#endif /* DELETED */

#ifdef WITH_REALTIME_SCHED
#warning "realtime scheduling is an experimental feature"
#ifdef HAVE_SCHED_H
#include <sched.h>              /* sched_set_scheduler(), SCHED_* */
#else /* NO_SCHED_H */
#error "<sched.h> is required by realtime scheduling but missing"
#endif /* HAVE_SCHED_H */
#endif /* WITH_REALTIME_SCHED */

#ifdef __cplusplus
extern "C"
{
#endif

/* Local macros (mostly for event handling) */

#define __QUEUE_EMPTY(pctrl) \
    (((pctrl)->event.size) <= 0) \
/* __QUEUE_EMPTY */

#define __QUEUE_FULL(pctrl) \
    (((pctrl)->event.size) >= (SBOX_EVENT_MAX)) \
/* __QUEUE_FULL */

#define __QUEUE_HEAD(pctrl) \
    ((pctrl)->event.list[((pctrl)->event.head)]) \
/* __QUEUE_HEAD */

#define __QUEUE_POP(pctrl) \
{{{ \
    if (!__QUEUE_EMPTY(pctrl)) \
    { \
        ++((pctrl)->event.head); \
        ((pctrl)->event.head) %= (SBOX_EVENT_MAX); \
        --((pctrl)->event.size); \
    } \
}}} /* __QUEUE_POP */

#define __QUEUE_PUSH(pctrl,elem) \
{{{ \
    assert(((pctrl)->event.size) < (SBOX_EVENT_MAX)); \
    (pctrl)->event.list[((((pctrl)->event.head) + \
        ((pctrl)->event.size)) % (SBOX_EVENT_MAX))] = (elem); \
    ++((pctrl)->event.size); \
}}} /* __QUEUE_PUSH */

#define __QUEUE_CLEAR(pctrl) \
{{{ \
    ((pctrl)->event.head) = ((pctrl)->event.size) = 0; \
    memset(((pctrl)->event.list), 0, (SBOX_EVENT_MAX) * sizeof(event_t)); \
}}} /* __QUEUE_CLEAR */

#define POST_EVENT(psbox,type,x...) \
{{{ \
    LOCK_ON_COND(psbox, EX, !__QUEUE_FULL(&((psbox)->ctrl))); \
    if (!HAS_RESULT(psbox)) \
    { \
        __QUEUE_PUSH(&((psbox)->ctrl), ((event_t){(S_EVENT ## type), {{x}}})); \
    } \
    UNLOCK(psbox); \
}}} /* POST_EVENT */

#define MONITOR_ERROR(psbox,x...) \
{{{ \
    WARN(x); \
    if (errno != ESRCH) \
    { \
        POST_EVENT((psbox), _ERROR, errno); \
        kill(-((psbox)->ctrl.pid), SIGKILL); \
    } \
}}} /* MONITOR_ERROR */

/* Macros for updating sandbox status / result */

#define __UPDATE_RESULT(psbox,res) \
{{{ \
    ((psbox)->result) = (result_t)(res); \
    DBUG("result: %s", s_result_name(((psbox)->result))); \
}}} /* __UPDATE_RESULT */

#define __UPDATE_STATUS(psbox,sta) \
{{{ \
    ((psbox)->status) = (status_t)(sta); \
    DBUG("status: %s", s_status_name(((psbox)->status))); \
}}} /* __UPDATE_STATUS */

#define UPDATE_RESULT(psbox,res) \
{{{ \
    LOCK(psbox, EX); \
    __UPDATE_RESULT(psbox, res); \
    UNLOCK(psbox); \
}}} /* UPDATE_RESULT */

#define UPDATE_STATUS(psbox,sta) \
{{{ \
    LOCK(psbox, EX); \
    __UPDATE_STATUS(psbox, sta); \
    UNLOCK(psbox); \
}}} /* UPDATE_STATUS */

/* Local function prototypes */

static void __sandbox_task_init(task_t *, const char * *);
static bool __sandbox_task_check(const task_t *);
static int  __sandbox_task_execute(task_t *);
static void __sandbox_task_fini(task_t *);

static void __sandbox_stat_init(stat_t *);
static void __sandbox_stat_update(sandbox_t *, const proc_t *);
static void __sandbox_stat_fini(stat_t *);

static void __sandbox_ctrl_init(ctrl_t *, thread_func_t);
static int  __sandbox_ctrl_add_monitor(ctrl_t *, thread_func_t);
static void __sandbox_ctrl_fini(ctrl_t *);

void * sandbox_watcher(sandbox_t *);
void * sandbox_profiler(sandbox_t *);

int 
sandbox_init(sandbox_t * psbox, const char * argv[])
{
    FUNC_BEGIN("%p,%p", psbox, argv);
    assert(psbox);
    
    if (psbox == NULL)
    {
        FUNC_RET("%d", -1);
    }
    
    psbox->lock = LOCK_INITIALIZER;
    LOCK(psbox, EX);
    __sandbox_task_init(&psbox->task, argv);
    __sandbox_stat_init(&psbox->stat);
    __sandbox_ctrl_init(&psbox->ctrl, (thread_func_t)sandbox_tracer);
    __sandbox_ctrl_add_monitor(&psbox->ctrl, (thread_func_t)sandbox_profiler);
    __sandbox_ctrl_add_monitor(&psbox->ctrl, (thread_func_t)sandbox_watcher);
    __UPDATE_RESULT(psbox, S_RESULT_PD);
    __UPDATE_STATUS(psbox, S_STATUS_PRE);
    UNLOCK(psbox);
    
    FUNC_RET("%d", 0);
}

int 
sandbox_fini(sandbox_t * psbox)
{
    FUNC_BEGIN("%p", psbox);
    assert(psbox);
    
    if (psbox == NULL)
    {
        FUNC_RET("%d", -1);
    }
    
    LOCK(psbox, EX);
    __UPDATE_RESULT(psbox, S_RESULT_PD);
    __UPDATE_STATUS(psbox, S_STATUS_FIN);
    __sandbox_task_fini(&psbox->task);
    __sandbox_stat_fini(&psbox->stat);
    __sandbox_ctrl_fini(&psbox->ctrl);
    UNLOCK(psbox);
    psbox->lock = LOCK_INITIALIZER;
    
    FUNC_RET("%d", 0);
}

bool 
sandbox_check(sandbox_t * psbox)
{
    FUNC_BEGIN("%p", psbox);
    assert(psbox);
    
    if (psbox == NULL)
    {
        FUNC_RET("%d", false);
    }
    
    LOCK(psbox, EX);
    
    /* Don't change the state of a running / blocking sandbox */
    if (!NOT_STARTED(psbox) && !IS_FINISHED(psbox))
    {
        UNLOCK(psbox);
        FUNC_RET("%d", false);
    }
    DBUG("passed sandbox status check");
    
    /* Clear previous statistics and status */
    if (IS_FINISHED(psbox))
    {
        __sandbox_stat_fini(&psbox->stat);
        __sandbox_stat_init(&psbox->stat);
    }
    
    __UPDATE_RESULT(psbox, S_RESULT_PD);
    __UPDATE_STATUS(psbox, S_STATUS_PRE);
    
    if (!__sandbox_task_check(&psbox->task))
    {
        UNLOCK(psbox);
        FUNC_RET("%d", false);
    }
    DBUG("passed task spec validation");
    
    if (psbox->ctrl.tracer.target == NULL)
    {
        UNLOCK(psbox);
        FUNC_RET("%d", false);
    }
    DBUG("passed ctrl tracer validation");
    
    if (psbox->ctrl.monitor[0].target == NULL)
    {
        UNLOCK(psbox);
        FUNC_RET("%d", false);
    }
    DBUG("passed ctrl monitor validation");
    
    __UPDATE_STATUS(psbox, S_STATUS_RDY);
    
    UNLOCK(psbox);
    FUNC_RET("%d", true);
}

result_t * 
sandbox_execute(sandbox_t * psbox)
{
    FUNC_BEGIN("%p", psbox);
    assert(psbox);
    
    if (psbox == NULL)
    {
        FUNC_RET("%p", (result_t *)NULL);
    }
    
    if (!sandbox_check(psbox))
    {
        WARN("sandbox pre-execution state check failed");
        FUNC_RET("%p", &psbox->result);
    }
    
#ifdef WITH_REALTIME_SCHED
    /* Turn on realtime scheduling */
    struct sched_param param = {1};
    if (getuid() == (uid_t)0)
    {
        if (sched_setscheduler(0, SCHED_RR, &param) != 0)
        {
            WARN("failed to apply realtime scheduling policy");
            FUNC_RET("%p", &psbox->result);
        }
        DBUG("applied realtime scheduling policy");
    }
#endif /* WITH_REALTIME_SCHED */
    
    LOCK(psbox, EX);
    
    /* Fork the prisoner process */
    psbox->ctrl.pid = fork();
    
    /* Execute the targeted program in the prisoner process */
    if (psbox->ctrl.pid == 0)
    {
        DBUG("entering: the prisoner process");
        UNLOCK(psbox);
        /* Start executing the targeted program */
        _exit(__sandbox_task_execute(&psbox->task));
    }
    
    /* Create all monitor threads */
    int all, i;
    for (i = all = 0; i < (SBOX_MONITOR_MAX); i++)
    {
        if (psbox->ctrl.monitor[i].target == NULL)
        {
            continue;
        }
        if (pthread_create(&psbox->ctrl.monitor[i].tid, NULL, 
            psbox->ctrl.monitor[i].target, (void *)psbox) != 0)
        {
            WARN("failed to create monitor thread at %p",
                psbox->ctrl.monitor[i].target);
            psbox->ctrl.monitor[i].target = NULL;
            continue; /* continue to create the rest monitor threads */
        }
        ++all;
        DBUG("created: monitor thread #%d at %p", all, 
            psbox->ctrl.monitor[i].target);
    }
    DBUG("created: %d monitor threads", all);
    
    /* Save current thread id */
    psbox->ctrl.tracer.tid = pthread_self();
    
    /* Start executing the main tracer thread */
    if (psbox->ctrl.pid > 0)
    {
        DBUG("forked the prisoner process as pid %d", psbox->ctrl.pid);
        __UPDATE_RESULT(psbox, S_RESULT_PD);
        __UPDATE_STATUS(psbox, S_STATUS_BLK);
        UNLOCK(psbox);
        psbox->ctrl.tracer.target(psbox);
    }
    else
    {
        WARN("error forking the prisoner process");
        __UPDATE_RESULT(psbox, S_RESULT_IE);
        __UPDATE_STATUS(psbox, S_STATUS_FIN);
        UNLOCK(psbox);
    }
    
    /* Join all monitor threads */
    int idx, cnt;
    for (i = idx = cnt = 0; i < (SBOX_MONITOR_MAX); i++)
    {
        if (psbox->ctrl.monitor[i].target == NULL)
        {
            continue;
        }
        ++idx;
        /* Final notification for the monitor thread to quit */
        pthread_kill(psbox->ctrl.monitor[i].tid, SIGEXIT);
        if (pthread_join(psbox->ctrl.monitor[i].tid, NULL) != 0)
        {
            WARN("failed to join monitor thread #%d", idx);
            if (pthread_cancel(psbox->ctrl.monitor[i].tid) != 0)
            {
                WARN("failed to cancel the %dth monitor thread", idx);
                continue; /* continue to join the rest monitor threads */
            }
        }
        ++cnt;
    }
    DBUG("joined %d of %d monitor threads", cnt, all);
    
    FUNC_RET("%p", &psbox->result);
}

static void 
__sandbox_task_init(task_t * ptask, const char * argv[])
{ 
    PROC_BEGIN("%p,%p", ptask, argv);
    assert(ptask);              /* argv could be NULL */
    
    memset(ptask, 0, sizeof(task_t));
    
    unsigned int argc = 0;
    unsigned int offset = 0;
    if (argv != NULL)
    {
        while (argv[argc] != NULL)
        {
            unsigned int delta  = strlen(argv[argc]) + 1;
            if (offset + delta >= sizeof(ptask->comm.buff))
            {
                break;
            }
            strcpy(ptask->comm.buff + offset, argv[argc]);
            ptask->comm.args[argc++] = offset;
            offset += delta;
        }
    }
    ptask->comm.buff[offset] = '\0';
    ptask->comm.args[argc] = -1;
    
    strcpy(ptask->jail, "/");
    ptask->uid = getuid();
    ptask->gid = getgid();
    ptask->ifd = STDIN_FILENO;
    ptask->ofd = STDOUT_FILENO;
    ptask->efd = STDERR_FILENO;
    ptask->quota[S_QUOTA_WALLCLOCK] = SBOX_QUOTA_INF;
    ptask->quota[S_QUOTA_CPU] = SBOX_QUOTA_INF;
    ptask->quota[S_QUOTA_MEMORY] = SBOX_QUOTA_INF;
    ptask->quota[S_QUOTA_DISK] = SBOX_QUOTA_INF;
    PROC_END();
}

static void 
__sandbox_task_fini(task_t * ptask)
{
    PROC_BEGIN("%p", ptask);
    assert(ptask);
    
    /* TODO */
    
    PROC_END();
}

static bool
__sandbox_task_check(const task_t * ptask)
{
    FUNC_BEGIN("%p", ptask);
    assert(ptask);
    
    /* 1. check uid, gid fields
     *   a) if user exists
     *   b) if group exists
     *   c) current user's privilege to set{uid,gid}
     */
    struct passwd * pw = NULL;
    if ((pw = getpwuid(ptask->uid)) == NULL)
    {
        FUNC_RET("%d", false);
    }
    DBUG("passed user identity test");
    
    struct group * gr = NULL;
    if ((gr = getgrgid(ptask->gid)) == NULL)
    {
        FUNC_RET("%d", false);
    }
    DBUG("passed group identity test");
    
    if ((getuid() != (uid_t)0) && ((getuid() != ptask->uid) || 
                                   (getgid() != ptask->gid)))
    {
        FUNC_RET("%d", false);
    }
    DBUG("passed set{uid,gid} privilege test");
    
    struct stat s;
    
    /* 2. check comm field
     *   a) if program file is an existing regular file
     *   b) if program file is executable by the specified user
     */
    if ((stat(ptask->comm.buff, &s) < 0) || !S_ISREG(s.st_mode))
    {
        FUNC_RET("%d", false);
    }
    
    if (!((S_IXUSR & s.st_mode) && (s.st_uid == ptask->uid)) && 
        !((S_IXGRP & s.st_mode) && (s.st_gid == ptask->gid)) && 
        !(S_IXOTH & s.st_mode) && !(ptask->uid == (uid_t)0))
    {
        FUNC_RET("%d", false);
    }
    
    DBUG("passed permission test of the targeted program");
    
    /* 3. check jail field (if jail is not "/")
     *   a) only super user can chroot!
     *   b) if jail path is accessible and readable
     *   c) if jail is a prefix to the targeted program command line
     */
    if (strcmp(ptask->jail, "/") != 0)
    {
        if (getuid() != (uid_t)0)
        {
            FUNC_RET("%d", false);
        }
        if (access(ptask->jail, X_OK | R_OK) < 0)
        {
            FUNC_RET("%d", false);
        }
        if ((stat(ptask->jail, &s) < 0) || !S_ISDIR(s.st_mode))
        {
            FUNC_RET("%d", false);
        }
        if (strstr(ptask->comm.buff, ptask->jail) != ptask->comm.buff)
        {
            FUNC_RET("%d", false);
        }
    }
    DBUG("passed jail validity test");
    
    /* 4. check ifd, ofd, efd are existing file descriptors
     *   a) if ifd is readable by current user
     *   b) if ofd and efd are writable by current user
     */
    if ((fstat(ptask->ifd, &s) < 0) || !(S_ISCHR(s.st_mode) || 
         S_ISREG(s.st_mode) || S_ISFIFO(s.st_mode)))
    {
        FUNC_RET("%d", false);
    }
    if (!((S_IRUSR & s.st_mode) && (s.st_uid == getuid())) && 
        !((S_IRGRP & s.st_mode) && (s.st_gid == getgid())) && 
        !(S_IROTH & s.st_mode) && !(getuid() == (uid_t)0))
    {
        FUNC_RET("%d", false);
    }
    DBUG("passed input channel validity test");
    
    if ((fstat(ptask->ofd, &s) < 0) || !(S_ISCHR(s.st_mode) ||
         S_ISREG(s.st_mode) || S_ISFIFO(s.st_mode)))
    {
        FUNC_RET("%d", false);
    }
    if (!((S_IWUSR & s.st_mode) && (s.st_uid == getuid())) && 
        !((S_IWGRP & s.st_mode) && (s.st_gid == getgid())) && 
        !(S_IWOTH & s.st_mode) && !(getuid() == (uid_t)0))
    {
        FUNC_RET("%d", false);
    }
    DBUG("passed output channel validity test");
    
    if ((fstat(ptask->efd, &s) < 0) || !(S_ISCHR(s.st_mode) ||
         S_ISREG(s.st_mode) || S_ISFIFO(s.st_mode)))
    {
        FUNC_RET("%d", false);
    }
    if (!((S_IWUSR & s.st_mode) && (s.st_uid == getuid())) && 
        !((S_IWGRP & s.st_mode) && (s.st_gid == getgid())) && 
        !(S_IWOTH & s.st_mode) && !(getuid() == (uid_t)0))
    {
        FUNC_RET("%d", false);
    }
    DBUG("passed error channel validity test");
    
    FUNC_RET("%d", true);
}

static int
__sandbox_task_execute(task_t * ptask)
{
    FUNC_BEGIN("%p", ptask);
    assert(ptask);
    
    /* Run the prisoner process in a separate process group */
    if (setsid() < 0)
    {
        WARN("failed to set session id");
        return EXIT_FAILURE;
    }
    
    /* Close fd's not used by the targeted program */
    int fd;
    for (fd = 0; fd < FILENO_MAX; fd++)
    {
        if ((fd == ptask->ifd) || (fd == ptask->ofd) || (fd == ptask->efd))
        {
            continue;
        }
        close(fd);
    }
    
    /* Redirect I/O channel */
    
    if (dup2(ptask->efd, STDERR_FILENO) < 0)
    {
        WARN("failed to redirect error channel");
        return EXIT_FAILURE;
    }
    DBUG("dup2: %d->%d", ptask->efd, STDERR_FILENO);
    
    if (dup2(ptask->ofd, STDOUT_FILENO) < 0)
    {
        WARN("failed to redirect output channel(s)");
        return EXIT_FAILURE;
    }
    DBUG("dup2: %d->%d", ptask->ofd, STDOUT_FILENO);
    
    if (dup2(ptask->ifd, STDIN_FILENO) < 0)
    {
        WARN("failed to redirect input channel(s)");
        return EXIT_FAILURE;
    }
    DBUG("dup2: %d->%d", ptask->ifd, STDIN_FILENO);
    
    /* Apply security restrictions */
    
    if (strcmp(ptask->jail, "/") != 0)
    {
        if (chdir(ptask->jail) < 0)
        {
            WARN("failed to chdir() to jail directory");
            return EXIT_FAILURE;
        }
        if (chroot(".") < 0)
        {
            WARN("failed to chroot() to jail directory");
            return EXIT_FAILURE;
        }
        DBUG("jail: \"%s\"", ptask->jail);
    }
    
    /* Change identity before executing the targeted program */
    
    if (setgid(ptask->gid) < 0)
    {
        WARN("failed to change group identity");
        return EXIT_FAILURE;
    }
    DBUG("setgid: %lu", (unsigned long)ptask->gid);
    
    if (setuid(ptask->uid) < 0)
    {
        WARN("failed to change owner identity");
        return EXIT_FAILURE;
    }
    DBUG("setuid: %lu", (unsigned long)ptask->uid);
    
    /* Prepare argument array to be passed to execve() */
    char * argv[SBOX_ARG_MAX] = {NULL};
    int argc = 0;
    while ((argc + 1 < (SBOX_ARG_MAX)) && (ptask->comm.args[argc] >= 0))
    {
        argv[argc] = ptask->comm.buff + ptask->comm.args[argc];
        argc++;
    }
    if (strcmp(ptask->jail, "/") != 0)
    {
        argv[0] += strlen(ptask->jail);
    }
    argv[argc] = NULL;
    
#ifndef NDEBUG
    argc = 0;
    while (argv[argc] != NULL)
    {
        DBUG("argv[%d]: \"%s\"", argc, argv[argc]);
        argc++;
    }
#endif /* NDEBUG */
    
    /* Unblock all signals for the prisoner process. */
    sigset_t sigmask;
    sigfillset(&sigmask);
    if (pthread_sigmask(SIG_UNBLOCK, &sigmask, NULL) != 0)
    {
        WARN("pthread_sigmask");
        return EXIT_FAILURE;
    }
    DBUG("unblocked all signals");
    
    /* Output quota is applied through the *setrlimit*. Because we might have 
     * already changed identity by this time, the hard limits should remain as 
     * they were. Thus we must invoke a *getrlimit* ahead of time to load 
     * original hard resource limit. */
    struct rlimit rlimval;
    
    /* Do NOT produce core dump files at all. */
    if (getrlimit(RLIMIT_CORE, &rlimval) != 0)
    {
        WARN("failed to getrlimit(RLIMIT_CORE)");
        return EXIT_FAILURE;
    }
    rlimval.rlim_cur = 0;
    if (setrlimit(RLIMIT_CORE, &rlimval) != 0)
    {
        WARN("failed to setrlimit(RLIMIT_CORE)");
        return EXIT_FAILURE;
    }
    DBUG("RLIMIT_CORE: %ld", rlimval.rlim_cur);
     
    /* Disk quota (bytes) */
    if (getrlimit(RLIMIT_FSIZE, &rlimval) != 0)
    {
        WARN("failed to getrlimit(RLIMIT_FSIZE)");
        return EXIT_FAILURE;
    }
    rlimval.rlim_cur = ptask->quota[S_QUOTA_DISK];
    if (setrlimit(RLIMIT_FSIZE, &rlimval) != 0)
    {
        WARN("failed to setrlimit(RLIMIT_FSIZE)");
        return EXIT_FAILURE;
    }
    DBUG("RLIMIT_FSIZE: %ld", rlimval.rlim_cur);
    
#ifdef DELETED
    /* Memory quota (bytes) */
    if (getrlimit(RLIMIT_AS, &rlimval) != 0)
    {
        return EXIT_FAILURE;
    }
    rlimval.rlim_cur = ptask->quota[S_QUOTA_MEMORY];
    if (setrlimit(RLIMIT_AS, &rlimval) != 0)
    {
        return EXIT_FAILURE;
    }
    DBUG("RLIMIT_AS: %ld", rlimval.rlim_cur);
#endif /* DELETED */
    
#ifdef DELETED
    /* CPU quota */
    struct itimerval itv;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    itv.it_value.tv_sec = ptask->quota[S_QUOTA_CPU] / 1000;
    itv.it_value.tv_usec = (ptask->quota[S_QUOTA_CPU] % 1000) * 1000;
    if (setitimer(ITIMER_PROF, &itv, NULL) < 0)
    {
        WARN("failed to setitimer(ITIMER_PROF)");
        return EXIT_FAILURE;
    }
#endif /* DELETED */
    
    /* Mark current process as traced */
    if (!trace_me())
    {
        WARN("trace_me");
        return EXIT_FAILURE;
    }
    
    /* Execute the targeted program */
    if (execve(argv[0], argv, NULL) != 0)
    {
        WARN("execve() failed unexpectedly");
        return errno;
    }
    
    /* According to Linux manual, the execve() function will NEVER return on
     * success, thus we should not be able to reach to this line of code! */
    return EXIT_FAILURE;
}

static void 
__sandbox_stat_init(stat_t * pstat)
{
    PROC_BEGIN("%p", pstat);
    assert(pstat);
    
    memset(pstat, 0, sizeof(stat_t));
    
    PROC_END();
}

static void 
__sandbox_stat_update(sandbox_t * psbox, const proc_t * pproc)
{
    PROC_BEGIN("%p,%p", psbox, pproc);
    assert(psbox && pproc);
    
    const struct timespec ZERO = {0, 0};
    struct timespec ts = ZERO;
    bool exceeded = false;
    
    LOCK(psbox, EX);
    
    /* mem_info */
    #define MEM_UPDATE(a,b) \
    {{{ \
        (a) = (b); \
        (a ## _peak) = (((a ## _peak) > (a)) ? (a ## _peak) : (a)); \
    }}} /* MEM_UPDATE */
    
    MEM_UPDATE(psbox->stat.mem_info.vsize, pproc->vsize);
    MEM_UPDATE(psbox->stat.mem_info.rss, pproc->rss * getpagesize());
    psbox->stat.mem_info.minflt = pproc->minflt;
    psbox->stat.mem_info.majflt = pproc->majflt;
    
    /* cpu_info */
    ts = ZERO;
    TS_INPLACE_ADD(ts, pproc->utime);
    TS_INPLACE_ADD(ts, pproc->stime);
    TS_UPDATE(psbox->stat.cpu_info.clock, ts);
    TS_UPDATE(psbox->stat.cpu_info.utime, pproc->utime);
    TS_UPDATE(psbox->stat.cpu_info.stime, pproc->stime);
    
    /* wallclock */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        UNLOCK(psbox);
        MONITOR_ERROR(psbox, "failed to get wallclock time");
        PROC_END();
    }
    
    /* Update the elapsed time stat of sandbox, also update the walllock
     * started time if it is not set already. */
    if (!TS_LESS(ZERO, psbox->stat.started))
    {
        psbox->stat.started = ts;
        psbox->stat.elapsed = ZERO;
    }
    else
    {
        TS_INPLACE_SUB(ts, psbox->stat.started);
        TS_UPDATE(psbox->stat.elapsed, ts);
    }
    
    UNLOCK(psbox);
    
    /* Compare peak memory usage against memory quota limit */
    LOCK(psbox, SH);
    if (psbox->stat.mem_info.vsize_peak > \
        psbox->task.quota[S_QUOTA_MEMORY])
    {
        DBUG("memory quota exceeded");
        UNLOCK(psbox);
        POST_EVENT(psbox, _QUOTA, S_QUOTA_MEMORY);
        exceeded = true;
    }
    else
    {
        UNLOCK(psbox);
    }
    
    /* Compare cpu clock time against cpu quota limit */
    LOCK(psbox, SH);
    if ((res_t)ts2ms(psbox->stat.cpu_info.clock) > \
        psbox->task.quota[S_QUOTA_CPU])
    {
        DBUG("cpu quota exceeded");
        UNLOCK(psbox);
        POST_EVENT(psbox, _QUOTA, S_QUOTA_CPU);
        exceeded = true;
    }
    else
    {
        UNLOCK(psbox);
    }
    
    /* Compare elapsed time against wallclock quota limit */
    LOCK(psbox, SH);
    if ((res_t)ts2ms(psbox->stat.elapsed) > 
        psbox->task.quota[S_QUOTA_WALLCLOCK])
    {
        DBUG("wallclock quota exceeded");
        UNLOCK(psbox);
        POST_EVENT(psbox, _QUOTA, S_QUOTA_WALLCLOCK);
        exceeded = true;
    }
    else
    {
        UNLOCK(psbox);
    }
    
    /* Stop targeted program to force event handling */
    if (exceeded)
    {
        trace_kill(pproc, SIGSTOP);
        trace_kill(pproc, SIGCONT);
    }
    
    PROC_END();
}

static void 
__sandbox_stat_fini(stat_t * pstat)
{
    PROC_BEGIN("%p", pstat);
    assert(pstat);
    
    /* TODO */
    
    PROC_END();
}

static void 
__sandbox_ctrl_init(ctrl_t * pctrl, thread_func_t tft)
{
    PROC_BEGIN("%p,%p", pctrl, tft);
    assert(pctrl && tft);
    
    memset(pctrl, 0, sizeof(ctrl_t));
    
    pctrl->policy.entry = (void *)sandbox_default_policy;
    pctrl->policy.data = 0L;
    memset(pctrl->monitor, 0, (SBOX_MONITOR_MAX) * sizeof(worker_t));
    memset(&pctrl->tracer, 0, sizeof(worker_t));
    pctrl->tracer.target = tft;
    __QUEUE_CLEAR(pctrl);
    
    PROC_END();
}

static void 
__sandbox_ctrl_fini(ctrl_t * pctrl)
{
    PROC_BEGIN("%p", pctrl);
    assert(pctrl);
    
    __QUEUE_CLEAR(pctrl);
    pctrl->tracer.target = NULL;
    memset(&pctrl->tracer, 0, sizeof(worker_t));
    memset(pctrl->monitor, 0, (SBOX_MONITOR_MAX) * sizeof(worker_t));
    
    PROC_END();
}

static int
__sandbox_ctrl_add_monitor(ctrl_t * pctrl, thread_func_t tfm)
{
    FUNC_BEGIN("%p,%p", pctrl, tfm);
    assert(pctrl && tfm);
    
    int i;
    for (i = 0; i < (SBOX_MONITOR_MAX); i++)
    {
        if (pctrl->monitor[i].target == NULL)
        {
            pctrl->monitor[i].target = tfm;
            break;
        }
    }
    
    FUNC_RET("%d", i);
}

void *
sandbox_watcher(sandbox_t * psbox)
{
    MONITOR_BEGIN(psbox);
    
    /* Temporary variables. */
    LOCK(psbox, SH);
    const pthread_t profiler_thread = psbox->ctrl.monitor[0].tid;
    const pid_t pid = psbox->ctrl.pid;
    ctrl_t * const pctrl = &psbox->ctrl;
    proc_t proc = {0};
    proc_bind(psbox, &proc);
    UNLOCK(psbox);
    
    siginfo_t w_info;
    int w_opt = WEXITED | WSTOPPED;
    int w_res = 0;
    long sc_stack[8] = {0};
    int sc_top = 0;
    
    /* Entering the watching loop */
    while ((w_res = waitid(P_PID, pid, &w_info, w_opt)) >= 0)
    {
        DBUG("---------------------------------------------------------------");
        DBUG("waitid(%d,%d,%p,%d): %d", P_PID, pid, &w_info, w_opt, w_res);
        
        UPDATE_STATUS(psbox, S_STATUS_BLK);
        
        /* Obtain signal info of the prisoner process */
        if (!proc_probe(pid, PROBE_SIGINFO, &proc))
        {
            MONITOR_ERROR(psbox, "failed to probe process: %d", pid);
            LOCK(psbox, SH);
            if (HAS_RESULT(psbox))
            {
                DBUG("exiting the watching loop");
                UNLOCK(psbox);
                break;
            }
            else
            {
                UNLOCK(psbox);
            }
        }
        
        /* Raise appropriate events judging each wait status */
        if (w_info.si_code == CLD_TRAPPED)
        {
            DBUG("wait: trapped (%d)", w_info.si_status);
            /* Generate appropriate event judging stop signal */
            switch (w_info.si_status)
            {
#ifdef DELETED
            case SIGPROF:       /* profile timer expired */
                if (proc.siginfo.si_code == SI_USER)
                {
                    goto report_signal;
                }
                POST_EVENT(psbox, _QUOTA, S_QUOTA_CPU);
                break;
#endif /* DELETED */
            case SIGXFSZ:       /* Output file size exceeded */
                /* As with SIGPROF, we should ideally test proc.siginfo.si_code
                 * here to see if the signal was sent by the kernel (i.e. reach
                 * of soft res limit) or by the user (i.e. kill -XFSZ). But 
                 * linux kernel (until 3.2) always sends SIGXFSZ with si_code
                 * == SI_USER (I located the bug in the linux kernel source at
                 * "mm/filemap.c/generic_write_checks()", and I'm preparing to 
                 * submit a trivial patch to the mailing list). 2011/12/05. */
                POST_EVENT(psbox, _QUOTA, S_QUOTA_DISK);
                goto update_signal;
                break;
            case SIGTRAP:
                /* Collect additional info of the prisoner process */
                if (!proc_probe(pid, PROBE_REGS | PROBE_OP, &proc))
                {
                    MONITOR_ERROR(psbox, "failed to probe process: %d", pid);
                    break;
                }
                /* Inspect opcode and tflags to see if the SIGTRAP signal was 
                 * due to a system call invocation or return. The subtleties are
                 * wrapped in the IS_SYSCALL and IS_SYSRET macros. */
                if (IS_SYSCALL(&proc) || IS_SYSRET(&proc))
                {
                    long sc = THE_SYSCALL(&proc);
                    
                    LOCK(psbox, EX);
                    psbox->stat.syscall = sc;
                    UNLOCK(psbox);
                    
                    if (sc != sc_stack[sc_top])
                    {
                        SET_IN_SYSCALL(&proc);
                        sc_stack[++sc_top] = sc;
                        POST_EVENT(psbox, _SYSCALL, sc, SYSCALL_ARG1(&proc),
                                                        SYSCALL_ARG2(&proc),
                                                        SYSCALL_ARG3(&proc),
                                                        SYSCALL_ARG4(&proc),
                                                        SYSCALL_ARG5(&proc),
                                                        SYSCALL_ARG6(&proc));
                    }
                    else
                    {
                        POST_EVENT(psbox, _SYSRET,  sc, SYSRET_RETVAL(&proc));
                        sc_stack[sc_top--] = 0;
                        CLR_IN_SYSCALL(&proc);
                    }
                }
                /* If the SIGTRAP was NOT synthetically generated by the trace
                 * system, it should be reported as a signaled event. */
                else if (proc.siginfo.si_code == SI_USER)
                {
                    if (NOT_WAIT_EXECVE(&proc))
                    {
                        goto report_signal;
                    }
                    DBUG("detected: post-execve SIGTRAP");
                }
#ifdef WITH_SOFTWARE_TSC
#warning "software tsc is an experimental feature"
                /* Update the tsc instructions counter */
                LOCK(psbox, EX);
                psbox->stat.cpu_info.tsc++;
                DBUG("tsc                         %010llu", \
                    psbox->stat.cpu_info.tsc);
                UNLOCK(psbox);
#endif /* WITH_SOFTWARE_TSC */
                break;
            default:            /* Other runtime errors */
                goto report_signal;
            }
        } /* trapped */
        else if ((w_info.si_code == CLD_STOPPED) || \
                 (w_info.si_code == CLD_KILLED) || \
                 (w_info.si_code == CLD_DUMPED))
        {
            DBUG("wait: signaled (%d)", w_info.si_status);
    report_signal:
            POST_EVENT(psbox, _SIGNAL, w_info.si_status, proc.siginfo.si_code);
    update_signal:
            LOCK(psbox, EX);
            psbox->stat.signal.signo = w_info.si_status;
            psbox->stat.signal.code = proc.siginfo.si_code;
            UNLOCK(psbox);
        }
        else if (w_info.si_code == CLD_EXITED)
        {
            DBUG("wait: exited (%d)", w_info.si_status);
            LOCK(psbox, EX);
            psbox->stat.exitcode = w_info.si_status;
            UNLOCK(psbox);
            POST_EVENT(psbox, _EXIT, w_info.si_status);
        }
        else
        {
            DBUG("wait: unknown (si_code = %d)", w_info.si_code);
            /* unknown event, should not reach here! */
        }
        
        /* Update resource usage statistics. */
        __sandbox_stat_update(psbox, &proc);
        pthread_kill(profiler_thread, SIGPROF);
        
        /* Deliver pending events to the policy module for investigation */
        LOCK(psbox, SH);
        while (!__QUEUE_EMPTY(pctrl))
        {
            /* Start investigating the event */
            DBUG("detected: event %s {%lu %lu %lu %lu %lu %lu %lu}",
                s_event_type_name(__QUEUE_HEAD(pctrl).type),
                __QUEUE_HEAD(pctrl).data.__bitmap__.A,
                __QUEUE_HEAD(pctrl).data.__bitmap__.B,
                __QUEUE_HEAD(pctrl).data.__bitmap__.C,
                __QUEUE_HEAD(pctrl).data.__bitmap__.D,
                __QUEUE_HEAD(pctrl).data.__bitmap__.E,
                __QUEUE_HEAD(pctrl).data.__bitmap__.F,
                __QUEUE_HEAD(pctrl).data.__bitmap__.G);
        
            /* Consult the sandbox policy to determine next action */
            ((policy_entry_t)pctrl->policy.entry)(&pctrl->policy, \
                &(__QUEUE_HEAD(pctrl)), &pctrl->action);
        
            DBUG("policy decided action: %s {%lu %lu}",
                s_action_type_name(pctrl->action.type),
                pctrl->action.data.__bitmap__.A,
                pctrl->action.data.__bitmap__.B);
            
            /* Perform the desired action */
            RELOCK(psbox, EX);
            switch (pctrl->action.type)
            {
            case S_ACTION_CONT:
                /* Drop the obsoleted event */
                __QUEUE_POP(pctrl);
                break;
            case S_ACTION_FINI:
                /* Terminate the prisoner process */
                __UPDATE_RESULT(psbox, pctrl->action.data._FINI.result);
                __QUEUE_CLEAR(pctrl);
                trace_kill(&proc, SIGKILL);
                break;
            default:
            case S_ACTION_KILL:
                __UPDATE_RESULT(psbox, pctrl->action.data._KILL.result);
                __QUEUE_CLEAR(pctrl);
                trace_kill(&proc, SIGKILL);
                break;
            }
            RELOCK(psbox, SH);
        }
        UNLOCK(psbox);
        
        /* Schedule for next trace */
#ifdef WITH_SOFTWARE_TSC
        if (!trace_next(&proc, TRACE_SINGLE_STEP))
#else /* WITHOUT_SOFTWARE_TSC */
        if (!trace_next(&proc, TRACE_SYSTEM_CALL))
#endif /* WITH_SOFTWARE_TSC */
        {
            MONITOR_ERROR(psbox, "failed to schedule next watch");
            LOCK(psbox, SH);
            if (HAS_RESULT(psbox))
            {
                DBUG("exiting the watching loop");
                UNLOCK(psbox);
                break;
            }
            else
            {
                UNLOCK(psbox);
            }
        }
        
        UPDATE_STATUS(psbox, S_STATUS_EXE);
    }
    
    UPDATE_STATUS(psbox, S_STATUS_FIN);
    
    LOCK(psbox, EX);
    if (!HAS_RESULT(psbox))
    {
        __UPDATE_RESULT(psbox, S_RESULT_BP);
    }
    UNLOCK(psbox);
    
    /* Notify the main tracer thread to return */
    trace_end(&proc);
    
    MONITOR_END(psbox);
}

void 
sandbox_default_policy(const policy_t * ppolicy, const event_t * pevent, 
               action_t * paction)
{
    PROC_BEGIN("%p,%p,%p", ppolicy, pevent, paction);
    assert(pevent && paction);
    
    switch (pevent->type)
    {
    case S_EVENT_SYSCALL:
        /* Unsupported system call modes (e.g. 32bit libsandbox observes system
         * calls from the 64bit table) are considered illegal. */
        if ((unsigned long)pevent->data._SYSCALL.scinfo >= \
            MAKE_WORD(0, SCMODE_MAX))
        {
            WARN("illegal system call mode");
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_RF}}};
            break;
        }
        /* Baseline (back) list of system calls. */
        switch (pevent->data._SYSCALL.scinfo)
        {
        case SC_FORK:
        case SC_VFORK:
        case SC_CLONE:
        case SC_PTRACE:
#ifdef SC_WAITPID
        case SC_WAITPID:
#endif /* SC_WAITPID */
        case SC_WAIT4:
        case SC_WAITID:
#ifdef __x86_64__
        case SC32_FORK:
        case SC32_VFORK:
        case SC32_CLONE:
        case SC32_PTRACE:
        case SC32_WAITPID:
        case SC32_WAIT4:
        case SC32_WAITID:
#endif /* __x86_64__ */
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_RF}}};
            break;
        default:
            *paction = (action_t){S_ACTION_CONT};
            break;
        }
        break;
    case S_EVENT_SYSRET:
        *paction = (action_t){S_ACTION_CONT};
        break;
    case S_EVENT_EXIT:
        switch (pevent->data._EXIT.code)
        {
        case EXIT_SUCCESS:
            *paction = (action_t){S_ACTION_FINI, {{S_RESULT_OK}}};
            break;
        default:
            *paction = (action_t){S_ACTION_FINI, {{S_RESULT_AT}}};
            break;
        }
        break;
    case S_EVENT_ERROR:
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_IE}}};
        break;
    case S_EVENT_SIGNAL:
        switch (pevent->data._SIGNAL.signo)
        {
        case SIGSTOP:
        case SIGCONT:
            *paction = (action_t){S_ACTION_CONT};
            break;
        default:
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_RT}}};
            break;
        }
        break;
    case S_EVENT_QUOTA:
        switch (pevent->data._QUOTA.type)
        {
        case S_QUOTA_WALLCLOCK:
        case S_QUOTA_CPU:
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_TL}}};
            break;
        case S_QUOTA_MEMORY:
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_ML}}};
            break;
        case S_QUOTA_DISK:
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_OL}}};
            break;
        }
        break;
    default:
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_IE}}};
        break;
    }
    
    PROC_END();
}

void *
sandbox_profiler(sandbox_t * psbox)
{
    MONITOR_BEGIN(psbox);
    
    /* Temporary variables */
    LOCK(psbox, SH);
    const pid_t pid = psbox->ctrl.pid;
    proc_t proc = {0};
    proc_bind(psbox, &proc);
    UNLOCK(psbox);
    
    /* Profiling is by means of sampling the resource usage of the prisoner 
     * process at a relatively high frequency, and raise out-of-quota events
     * as soon as they happen. Other monitor threads may trigger profiling
     * by sending SIGSTAT signals to the profiler thread. */
    
    clockid_t clockid;
    struct timespec ts;
    
    /* Obtain the cpu clock id of the prisoner process */
    if (clock_getcpuclockid(pid, &clockid) != 0)
    {
        MONITOR_ERROR(psbox, "failed to get the prisoner's cpu clock id");
        MONITOR_END(psbox);
    }
    
    /* Wait until the watcher thread has seen the initial SIGTRAP, and that the
     * prisoner process has mapped in the executable through execve(). This is
     * essential for correct memory profiling, because in between fork() and
     * execve(), the memory space of the prisoner process is duplicated from
     * the process running libsandbox, whose size does not represent the memory
     * usage of the prisoner process, and may exceed the (mem) quota! */
    
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGEXIT);
    sigaddset(&sigmask, SIGSTAT);
    sigaddset(&sigmask, SIGPROF);
    
    LOCK_ON_COND(psbox, SH, !IS_BLOCKED(psbox));
    
    while (!IS_FINISHED(psbox))
    {
        UNLOCK(psbox);
        
        int signo;
        siginfo_t siginfo;
        if ((signo = sigwaitinfo(&sigmask, &siginfo)) < 0)
        {
            WARN("failed to sigwaitinfo()");
            goto check_status;
        }
        
        switch (signo)
        {
        case SIGSTAT:
            /* Collect stat of the prisoner process */
            if (!proc_probe(pid, PROBE_STAT, &proc))
            {
                WARN("failed to probe process: %d", pid);
                /* Do NOT raise monitor error here because the prisoner process
                 * may have gone making proc_probe() to fail. */
                break;
            }
            
            /* Update resource usage statistics. */
            __sandbox_stat_update(psbox, &proc);
            
            /* NOTE: do NOT break here, proceed to cpu clock profiling */
        case SIGPROF:
            /* Sample the cpu clock time of the prisoner process */
            if (clock_gettime(clockid, &ts) != 0)
            {
                WARN("failed to get the prisoner's cpu clock time");
                /* Do NOT raise monitor error here because the prisoner process
                 * may have gone making the clock invalid. */
                break;
            }
            /* Update sandbox stat with the sampled data */
            LOCK(psbox, EX);
            TS_UPDATE(psbox->stat.cpu_info.clock, ts);
            RELOCK(psbox, SH);
            if ((res_t)ts2ms(psbox->stat.cpu_info.clock) >
                psbox->task.quota[S_QUOTA_CPU])
            {
                DBUG("cpu quota exceeded");
                UNLOCK(psbox);
                POST_EVENT(psbox, _QUOTA, S_QUOTA_CPU);
                trace_kill(&proc, SIGSTOP);
                trace_kill(&proc, SIGCONT);
                /* Block the profiling signal upon the first out-of-quota (cpu)
                 * event. This avoids jamming the event queue in case the user-
                 * specified policy module ignores out-of-quota events. */
                sigdelset(&sigmask, SIGPROF);
            }
            else
            {
                UNLOCK(psbox);
            }
            break;
        case SIGEXIT:
            if (siginfo.si_code != SI_QUEUE)
            {
                DBUG("termination signal %d", signo);
                break;
            }
            /* Extract the real signal from siginfo */
            switch (siginfo.si_int)
            {
            case SIGTERM:
            case SIGQUIT:
            case SIGINT:
                /* These signals are forwarded to the prisoner process. This
                 * allows shell programs running libsandbox to respond to user 
                 * emitted signals such as <Ctrl> + C (i.e. SIGINT). */
                errno = EINTR;
                WARN("termination signal %d", siginfo.si_int);
                trace_kill(&proc, signo);
                break;
            default:
                /* When the process running libsandbox is interrupted by an
                 * unexpected signal, try to terminate the prisoner process.
                 * Since this is not the fault of the prisoner process, the 
                 * signal is reported as a monitor error. */
                errno = EINTR;
                MONITOR_ERROR(psbox, "unexpected signal %d", siginfo.si_int);
                break;
            }
            /* Do NOT exit the profiling loop immediately. Instead, go back to
             * the front to verify if the watcher thread is already finished. */
            break;
        }

check_status:
        LOCK(psbox, SH);
    }
    UNLOCK(psbox);
    
    MONITOR_END(psbox);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
