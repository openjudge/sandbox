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

#include "sandbox.h"
#include "platform.h"
#include "internal.h" 
#include "config.h"

#include <errno.h>              /* ECHILD, EINVAL */
#include <grp.h>                /* struct group, getgrgid() */
#include <pwd.h>                /* struct passwd, getpwuid() */
#include <pthread.h>            /* pthread_{cond_,mutex_,sigmask}*(), ... */
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

#define _QUEUE_EMPTY(pctrl) \
    ((((pctrl)->event).size) <= 0) \
/* _QUEUE_EMPTY */

#define _QUEUE_FULL(pctrl) \
    ((((pctrl)->event).size) >= (SBOX_EVENT_MAX)) \
/* _QUEUE_FULL */

#define _QUEUE_FRONT(pctrl) \
    (((pctrl)->event).queue[(((pctrl)->event).head)]) \
/* _QUEUE_FRONT */

#define _QUEUE_POP(pctrl) \
{{{ \
    if (!_QUEUE_EMPTY(pctrl)) \
    { \
        (((pctrl)->event).head) += 1; \
        (((pctrl)->event).head) %= (SBOX_EVENT_MAX); \
        (((pctrl)->event).size) -= 1; \
    } \
}}} /* _QUEUE_POP */

#define _QUEUE_PUSH(pctrl,e) \
{{{ \
    assert((((pctrl)->event).size) < (SBOX_EVENT_MAX)); \
    ((pctrl)->event).queue[(((((pctrl)->event).head) + \
        (((pctrl)->event).size)) % (SBOX_EVENT_MAX))] = (e); \
    (((pctrl)->event).size) += 1; \
}}} /* _QUEUE_PUSH */

#define _QUEUE_CLEAR(pctrl) \
{{{ \
    (((pctrl)->event).head) = (((pctrl)->event).size) = 0; \
    memset((((pctrl)->event).queue), 0, (SBOX_EVENT_MAX) * sizeof(event_t)); \
    pthread_cond_broadcast(&((pctrl)->sched)); \
}}} /* _QUEUE_CLEAR */

#define POST_EVENT(psbox,type,x...) \
{{{ \
    P(&((psbox)->ctrl.mutex)); \
    P(&((psbox)->mutex)); \
    if (!HAS_RESULT(psbox)) \
    { \
        V(&((psbox)->mutex)); \
        while (_QUEUE_FULL(&((psbox)->ctrl))) \
        { \
            pthread_cond_wait(&((psbox)->ctrl.sched), &((psbox)->ctrl.mutex)); \
        } \
        _QUEUE_PUSH(&((psbox)->ctrl), ((event_t){(S_EVENT ## type), {{x}}})); \
    } \
    else \
    { \
        V(&((psbox)->mutex)); \
    } \
    pthread_cond_broadcast(&((psbox)->ctrl.sched)); \
    V(&((psbox)->ctrl.mutex)); \
}}} /* POST_EVENT */

#define WAIT4_EVENT_HANDLING(psbox, pproc) \
{{{ \
    P(&((psbox)->ctrl.mutex)); \
    P(&((psbox)->mutex)); \
    while (!_QUEUE_EMPTY(&((psbox)->ctrl)) && !HAS_RESULT(psbox)) \
    { \
        DBUG("waiting for %d event(s) to be handled", \
            (((psbox)->ctrl.event).size)); \
        V(&((psbox)->mutex)); \
        pthread_cond_wait(&((psbox)->ctrl.sched), &((psbox)->ctrl.mutex)); \
        P(&((psbox)->mutex)); \
    } \
    if (HAS_RESULT(psbox)) \
    { \
        DBUG("exiting the tracing loop"); \
        V(&((psbox)->mutex)); \
        V(&((psbox)->ctrl.mutex)); \
        break; \
    } \
    V(&((psbox)->mutex)); \
    V(&((psbox)->ctrl.mutex)); \
}}} /* WAIT4_EVENT_HANDLING */

#define FLUSH_ALL_EVENTS(psbox) \
{{{ \
    P(&((psbox)->ctrl.mutex)); \
    P(&((psbox)->mutex)); \
    while (!_QUEUE_EMPTY(&((psbox)->ctrl)) && !HAS_RESULT(psbox)) \
    { \
        pthread_cond_wait(&((psbox)->update), &((psbox)->mutex)); \
    } \
    _QUEUE_CLEAR(&((psbox)->ctrl)); \
    V(&((psbox)->mutex)); \
    V(&((psbox)->ctrl.mutex)); \
}}} /* FLUSH_ALL_EVENTS */

/* Macros for updating sandbox status / result */

#define _UPDATE_RESULT(psbox,res) \
{{{ \
    ((psbox)->result) = (result_t)(res); \
    pthread_cond_broadcast(&((psbox)->update)); \
    pthread_cond_broadcast(&((psbox)->ctrl.sched)); \
    DBUG("result: %s", s_result_name(((psbox)->result))); \
}}} /* _UPDATE_RESULT */

#define _UPDATE_STATUS(psbox,sta) \
{{{ \
    ((psbox)->status) = (status_t)(sta); \
    pthread_cond_broadcast(&((psbox)->update)); \
    pthread_cond_broadcast(&((psbox)->ctrl.sched)); \
    DBUG("status: %s", s_status_name(((psbox)->status))); \
}}} /* _UPDATE_STATUS */

#define UPDATE_RESULT(psbox,res) \
{{{ \
    P(&((psbox)->mutex)); \
    _UPDATE_RESULT(psbox, res); \
    V(&((psbox)->mutex)); \
}}} /* UPDATE_RESULT */

#define UPDATE_STATUS(psbox,sta) \
{{{ \
    P(&((psbox)->mutex)); \
    _UPDATE_STATUS(psbox, sta); \
    V(&((psbox)->mutex)); \
}}} /* UPDATE_STATUS */

/* Local function prototypes */

static void __sandbox_task_init(task_t *, const char * *);
static bool __sandbox_task_check(const task_t *);
static int  __sandbox_task_execute(task_t *);
static void __sandbox_task_fini(task_t *);

static void __sandbox_stat_init(stat_t *);
static void __sandbox_stat_fini(stat_t *);

static void __sandbox_ctrl_init(ctrl_t *, thread_func_t);
static int  __sandbox_ctrl_add_monitor(ctrl_t *, thread_func_t);
static void __sandbox_ctrl_fini(ctrl_t *);

static void * __sandbox_tracer(sandbox_t *);
static void * __sandbox_watcher(sandbox_t *);
static void * __sandbox_profiler(sandbox_t *);

int 
sandbox_init(sandbox_t * psbox, const char * argv[])
{
    FUNC_BEGIN("%p,%p", psbox, argv);
    assert(psbox);
    
    if (psbox == NULL)
    {
        FUNC_RET("%d", -1);
    }
    
    pthread_mutex_init(&psbox->mutex, NULL);
    P(&psbox->mutex);
    pthread_cond_init(&psbox->update, NULL);
    __sandbox_task_init(&psbox->task, argv);
    __sandbox_stat_init(&psbox->stat);
    __sandbox_ctrl_init(&psbox->ctrl, (thread_func_t)trace_main);
    __sandbox_ctrl_add_monitor(&psbox->ctrl, (thread_func_t)__sandbox_tracer);
    __sandbox_ctrl_add_monitor(&psbox->ctrl, (thread_func_t)__sandbox_watcher);
    __sandbox_ctrl_add_monitor(&psbox->ctrl, (thread_func_t)__sandbox_profiler);
    V(&psbox->mutex);
    
    UPDATE_RESULT(psbox, S_RESULT_PD);
    UPDATE_STATUS(psbox, S_STATUS_PRE);
    
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
    
    UPDATE_RESULT(psbox, S_RESULT_PD);
    UPDATE_STATUS(psbox, S_STATUS_FIN);
    
    P(&psbox->mutex);
    __sandbox_task_fini(&psbox->task);
    __sandbox_stat_fini(&psbox->stat);
    __sandbox_ctrl_fini(&psbox->ctrl);
    pthread_cond_destroy(&psbox->update);
    V(&psbox->mutex);
    pthread_mutex_destroy(&psbox->mutex);
    
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
    
    P(&psbox->mutex);
    
    /* Don't change the state of a running / blocking sandbox */
    if (!NOT_STARTED(psbox) && !IS_FINISHED(psbox))
    {
        V(&psbox->mutex);
        FUNC_RET("%d", false);
    }
    DBUG("passed sandbox status check");
    
    /* Clear previous statistics and status */
    if (IS_FINISHED(psbox))
    {
        __sandbox_stat_fini(&psbox->stat);
        __sandbox_stat_init(&psbox->stat);
        _UPDATE_RESULT(psbox, S_RESULT_PD);
    }
    
    /* Update status to PRE */
    if (psbox->status != S_STATUS_PRE)
    {
        _UPDATE_STATUS(psbox, S_STATUS_PRE);
    }
    
    if (!__sandbox_task_check(&psbox->task))
    {
        V(&psbox->mutex);
        FUNC_RET("%d", false);
    }
    DBUG("passed task spec validation");
    
    if (psbox->ctrl.tracer == NULL)
    {
        V(&psbox->mutex);
        FUNC_RET("%d", false);
    }
    DBUG("passed ctrl tracer validation");
    
    if (psbox->ctrl.monitor[0] == NULL)
    {
        V(&psbox->mutex);
        FUNC_RET("%d", false);
    }
    DBUG("passed ctrl monitor validation");
    
    /* Update status to RDY */
    if (psbox->status != S_STATUS_RDY)
    {
        _UPDATE_STATUS(psbox, S_STATUS_RDY);
    }
    V(&psbox->mutex);
    
    FUNC_RET("%d", true);
}

result_t * 
sandbox_execute(sandbox_t * psbox)
{
    FUNC_BEGIN("%p", psbox);
    assert(psbox);
    
    if (psbox == NULL)
    {
        FUNC_RET("%p", NULL);
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
            WARN("failed setting realtime scheduling policy");
            FUNC_RET("%p", &psbox->result);
        }
        DBUG("applied realtime scheduling policy");
    }
#endif /* WITH_REALTIME_SCHED */
    
    /* Block as many signals as possible */
    sigset_t sigset, oldset;
    sigfillset(&sigset);
    if (pthread_sigmask(SIG_BLOCK, &sigset, &oldset) != 0)
    {
        WARN("pthread_sigmask");
        FUNC_RET("%p", &psbox->result);
    }
    DBUG("blocked all signals");
    
    /* There are two mutexes in each sandbox object, one for accessing the stat
     * / status / result of the sandbox (i.e. psbox->mutex) and the other for 
     * manipulating the controller (i.e. psbox->ctrl.mutex). To avoid racing 
     * conditions among monitors, the *specified* order to lock the two mutexes
     * is to first lock the controller then the sandbox itself. */    
    
    P(&psbox->ctrl.mutex);
    P(&psbox->mutex);
    
    /* Create all monitoring threads */
    pthread_t tid[SBOX_MONITOR_MAX] = {0};
    int cnt = 0, i;
    for (i = 0; i < SBOX_MONITOR_MAX; i++)
    {
        if (psbox->ctrl.monitor[i] == NULL)
        {
            continue;
        }
        if (pthread_create(&tid[cnt], NULL, psbox->ctrl.monitor[i], 
            (void *)psbox) != 0)
        {
            WARN("failed to create monitoring thread at %p",
                psbox->ctrl.monitor[i]);
            continue; /* continue to create the rest monitoring threads */
        }
        DBUG("created monitoring thread #%d at %p", cnt, psbox->ctrl.monitor[i]);
        ++cnt;
    }
    DBUG("created %d monitoring threads", cnt);
    
    /* Fork the prisoner process */
    psbox->ctrl.pid = fork();
    
    /* Execute the targeted program in the prisoner process */
    if (psbox->ctrl.pid == 0)
    {
        DBUG("entering: the prisoner process");
        V(&psbox->mutex);
        V(&psbox->ctrl.mutex);
        /* Start executing the targeted program */
        _exit(__sandbox_task_execute(&psbox->task));
    }
    
    /* Start executing the tracing thread */
    if (psbox->ctrl.pid > 0)
    {
        DBUG("forked the prisoner process as pid %d", psbox->ctrl.pid);
        psbox->ctrl.tid = pthread_self();
        V(&psbox->mutex);
        V(&psbox->ctrl.mutex);
        UPDATE_RESULT(psbox, S_RESULT_PD);
        UPDATE_STATUS(psbox, S_STATUS_BLK);
        psbox->ctrl.tracer(psbox);
    }
    else
    {
        WARN("error forking the prisoner process");
        V(&psbox->mutex);
        V(&psbox->ctrl.mutex);
        UPDATE_RESULT(psbox, S_RESULT_IE);
        UPDATE_STATUS(psbox, S_STATUS_FIN);
    }
    
    /* Join all monitoring threads */
    for (i = 0; i < cnt; i++)
    {
        if (pthread_join(tid[i], NULL) != 0)
        {
            WARN("failed to join monitoring thread #%d", i);
            if (pthread_cancel(tid[i]) != 0)
            {
                WARN("failed to cancel the %dth monitoring thread", i);
                continue; /* continue to join the rest monitoring threads */
            }
        }
    }
    DBUG("joined %d of %d monitoring threads", i, cnt);
    
    /* Restore old signal mask */
    if (pthread_sigmask(SIG_SETMASK, &oldset, NULL) != 0)
    {
        WARN("pthread_sigmask");
    }
    DBUG("restored old signal mask");
    
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
        WARN("failed setting session id");
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
        WARN("failed redirecting error channel");
        return EXIT_FAILURE;
    }
    DBUG("dup2: %d->%d", ptask->efd, STDERR_FILENO);
    
    if (dup2(ptask->ofd, STDOUT_FILENO) < 0)
    {
        WARN("failed redirecting output channel(s)");
        return EXIT_FAILURE;
    }
    DBUG("dup2: %d->%d", ptask->ofd, STDOUT_FILENO);
    
    if (dup2(ptask->ifd, STDIN_FILENO) < 0)
    {
        WARN("failed redirecting input channel(s)");
        return EXIT_FAILURE;
    }
    DBUG("dup2: %d->%d", ptask->ifd, STDIN_FILENO);
    
    /* Apply security restrictions */
    
    if (strcmp(ptask->jail, "/") != 0)
    {
        if (chdir(ptask->jail) < 0)
        {
            WARN("failed switching to jail directory");
            return EXIT_FAILURE;
        }
        if (chroot(".") < 0)
        {
            WARN("failed chroot to jail directory");
            return EXIT_FAILURE;
        }
        DBUG("jail: \"%s\"", ptask->jail);
    }
    
    /* Change identity before executing the targeted program */
    
    if (setgid(ptask->gid) < 0)
    {
        WARN("changing group identity");
        return EXIT_FAILURE;
    }
    DBUG("setgid: %lu", (unsigned long)ptask->gid);
    
    if (setuid(ptask->uid) < 0)
    {
        WARN("changing owner identity");
        return EXIT_FAILURE;
    }
    DBUG("setuid: %lu", (unsigned long)ptask->uid);
    
    /* Prepare argument array to be passed to execve() */
    char * argv[SBOX_ARG_MAX] = {NULL};
    int argc = 0;
    while ((argc + 1 < SBOX_ARG_MAX) && (ptask->comm.args[argc] >= 0))
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
    sigset_t sigset;
    sigfillset(&sigset);
    if (pthread_sigmask(SIG_UNBLOCK, &sigset, NULL) != 0)
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
        WARN("getting RLIMIT_CORE");
        return EXIT_FAILURE;
    }
    rlimval.rlim_cur = 0;
    if (setrlimit(RLIMIT_CORE, &rlimval) != 0)
    {
        WARN("setting RLIMIT_CORE");
        return EXIT_FAILURE;
    }
    DBUG("RLIMIT_CORE: %ld", rlimval.rlim_cur);
     
    /* Disk quota (bytes) */
    if (getrlimit(RLIMIT_FSIZE, &rlimval) != 0)
    {
        WARN("getting RLIMIT_FSIZE");
        return EXIT_FAILURE;
    }
    rlimval.rlim_cur = ptask->quota[S_QUOTA_DISK];
    if (setrlimit(RLIMIT_FSIZE, &rlimval) != 0)
    {
        WARN("setting RLIMIT_FSIZE");
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
        WARN("setting ITIMER_PROF");
        return EXIT_FAILURE;
    }
#endif /* DELETED */
    
    /* Enter tracing mode */
    if (!trace_self())
    {
        WARN("trace_self");
        return EXIT_FAILURE;
    }
    
    /* Execute the targeted program */
    if (execve(argv[0], argv, NULL) != 0)
    {
        WARN("execve failed unexpectedly");
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
    pthread_mutex_init(&pctrl->mutex, NULL);
    
    P(&pctrl->mutex);
    pthread_cond_init(&pctrl->sched, NULL);
    pctrl->policy.entry = (void *)sandbox_default_policy;
    pctrl->policy.data = 0L;
    memset(pctrl->monitor, 0, (SBOX_MONITOR_MAX) * sizeof(thread_func_t));
    pctrl->tracer = tft;
    _QUEUE_CLEAR(pctrl);
    V(&pctrl->mutex);
    
    PROC_END();
}

static void 
__sandbox_ctrl_fini(ctrl_t * pctrl)
{
    PROC_BEGIN("%p", pctrl);
    assert(pctrl);
    
    P(&pctrl->mutex);
    _QUEUE_CLEAR(pctrl);
    pctrl->tracer = NULL;
    memset(pctrl->monitor, 0, (SBOX_MONITOR_MAX) * sizeof(thread_func_t));
    pthread_cond_destroy(&pctrl->sched);
    V(&pctrl->mutex);
    
    pthread_mutex_destroy(&pctrl->mutex);
    
    PROC_END();
}

static int
__sandbox_ctrl_add_monitor(ctrl_t * pctrl, thread_func_t tfm)
{
    FUNC_BEGIN("%p,%p", pctrl, tfm);
    assert(pctrl && tfm);
    
    P(&pctrl->mutex);
    int i;
    for (i = 0; i < (SBOX_MONITOR_MAX); i++)
    {
        if (pctrl->monitor[i] == NULL)
        {
            pctrl->monitor[i] = tfm;
            break;
        }
    }
    pthread_cond_broadcast(&pctrl->sched);
    V(&pctrl->mutex);
    
    FUNC_RET("%d", i);
}

static void *
__sandbox_profiler(sandbox_t * psbox)
{
    MONITOR_BEGIN(psbox);
    
    /* Temporary variables */
    ctrl_t * pctrl = &psbox->ctrl;
    pid_t pid = pctrl->pid;
    struct timespec ts;
    
    /* Get wallclock start time */
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        WARN("failed getting wallclock start time");
        POST_EVENT(psbox, _ERROR, errno, pthread_self());
        MONITOR_END(psbox);
    }
    
    P(&psbox->mutex);
    psbox->stat.started = ts;
    V(&psbox->mutex);
    
    /* Profiling is by means of sampling the cpu clock time of the prisoner 
     * process at a relatively high frequency, i.e. 1kHz, and raise an out-of-
     * quota (cpu) event as soon as it happens. The profiling timer will emit
     * RT_SIGPROF signals to the profiler thread. */
    
    struct itimerspec its;
    struct sigevent sev;
    
    timer_t sw_timer;           /* stop-watch timer */
    timer_t pf_timer;           /* profiling timer */
    
    /* Stop-watch timer for updating elapsed time and raising out-of-quota
     * (wallclock and memory) events. */
    
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = ms2ns(200);
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = ms2ns(200);
    
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_value.sival_ptr = &sw_timer;
    sev.sigev_signo = RT_SIGSTAT;
    sev.sigev_notify = SIGEV_SIGNAL;
    
    if (timer_create(CLOCK_REALTIME, &sev, &sw_timer) != 0)
    {
        WARN("failed creating stop-watch timer");
        POST_EVENT(psbox, _ERROR, errno, pthread_self());
        MONITOR_END(psbox);
    }
    
    if (timer_settime(sw_timer, 0, &its, NULL) != 0)
    {
        WARN("failed setting up stop-watch timer");
        POST_EVENT(psbox, _ERROR, errno, pthread_self());
        MONITOR_END(psbox);
    }
    
    DBUG("started stop-watch timer at 5Hz");
    
    /* Obtain the cpu clock id of the prisoner process */
    clockid_t clockid;
    if (clock_getcpuclockid(pid, &clockid) != 0)
    {
        WARN("failed getting prisoner's cpu clock id");
        kill(-pid, SIGKILL);
        /* Do NOT post event here because the prisoner proceess may
         * have gone making the clock invalid. */
        MONITOR_END(psbox);
    }
    
    /* Profiling timer for emitting RT_SIGPROF signals. The timer will fire 
     * periodically at a freq of PROF_FREQ (Hz). At a freq of 0.25kHz, the cpu 
     * clock of the prisoner process is sampled once every 4msec, and the time
     * accuracy of profiling is expected to be within 10msec. After the first 
     * out-of-quota (cpu) event, the profiling timer will be slowed down by a 
     * factor of PROF_FACTOR to avoid jamming the event queue. */
    
    #define PROF_FREQ 250 /* 0.25kHz */
    #define PROF_FACTOR 250 /* 250x */
    
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = ms2ns(1000) / PROF_FREQ;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = ms2ns(10); /* warmup time */
    
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_value.sival_ptr = &pf_timer;
    sev.sigev_signo = RT_SIGPROF;
    sev.sigev_notify = SIGEV_SIGNAL;
    
    if (timer_create(CLOCK_REALTIME, &sev, &pf_timer) != 0)
    {
        WARN("failed creating profiling timer");
        POST_EVENT(psbox, _ERROR, errno, pthread_self());
        MONITOR_END(psbox);
    }
    
    if (timer_settime(pf_timer, 0, &its, NULL) != 0)
    {
        WARN("failed setting up profiling timer");
        POST_EVENT(psbox, _ERROR, errno, pthread_self());
        MONITOR_END(psbox);
    }
    
    DBUG("started profiling timer at %dHz", PROF_FREQ);
    
    /* Wait and handle signals */
    
    proc_t proc = {0};
    proc_bind(psbox, &proc);
    
    sigset_t sigset;
    sigfillset(&sigset);
    sigdelset(&sigset, SIGCHLD);
    sigdelset(&sigset, SIGPWR);
    sigdelset(&sigset, SIGWINCH);
    sigdelset(&sigset, SIGURG);
    
    P(&psbox->mutex);
    while (!IS_FINISHED(psbox))
    {
        V(&psbox->mutex);
        
        int signo;
        siginfo_t siginfo;
        if ((signo = sigwaitinfo(&sigset, &siginfo)) < 0)
        {
            WARN("failed waiting for signals");
            goto check_status;
        }
        
        if (signo == RT_SIGPROF)
        {
            /* Sample the cpu clock time of the prisoner process */
            if (clock_gettime(clockid, &ts) != 0)
            {
                WARN("failed getting prisoner's cpu clock time");
                kill(-pid, SIGKILL);
                /* Do NOT post event here because the prisoner proceess may
                 * have gone making the clock invalid. */
                goto check_status;
            }
            /* Update sandbox stat with the sampled data */
            P(&psbox->mutex);
            psbox->stat.cpu_info.clock = ts;
            if ((res_t)ts2ms(ts) > psbox->task.quota[S_QUOTA_CPU])
            {
                DBUG("cpu quota exceeded");
                V(&psbox->mutex);
                /* NOTE: unlock the sandbox before posting events */
                POST_EVENT(psbox, _QUOTA, S_QUOTA_CPU);
                /* Slow down profiling timer after the first out-of-quota (cpu)
                 * event. This is to avoid jaming the event queue in case the
                 * use-specified policy module ignores out-of-quota events. */
                its.it_interval.tv_nsec = ms2ns(1000) / PROF_FREQ * PROF_FACTOR;
                its.it_interval.tv_sec = 0;
                its.it_value.tv_sec = 0;
                its.it_value.tv_nsec = 0;
                timer_settime(pf_timer, 0, &its, NULL);
            }
            else
            {
                V(&psbox->mutex);
            }
            goto check_status;
        }
        
        if (signo == RT_SIGSTAT)
        {
            /* Collect stat of prisoner process */        
            if (!proc_probe(pid, PROBE_STAT, &proc))
            {
                WARN("failed to probe process: %d", pid);
                /* Do NOT post event here because the prisoner proceess may
                 * have gone making the procfs entry invalid. */
                kill(-pid, SIGKILL);
                goto check_status;
            }
            
            P(&psbox->mutex);

            /* cpu_info */
            #define CPU_UPDATE(a,x) \
            {{{ \
                ((a).tv_sec) = ((((x).tv_sec) > ((a).tv_sec)) ? \
                                ((x).tv_sec) : ((a).tv_sec)); \
                ((a).tv_nsec) = (((((x).tv_sec) > ((a).tv_sec)) || \
                                  (((((x).tv_sec) == ((a).tv_sec))) && \
                                   ((((x).tv_nsec) > ((a).tv_nsec))))) ? \
                                 ((x).tv_nsec) : ((a).tv_nsec)); \
            }}} /* CPU_UPDATE */
            
            CPU_UPDATE(psbox->stat.cpu_info.utime, proc.utime);
            CPU_UPDATE(psbox->stat.cpu_info.stime, proc.stime);
            
            /* mem_info */
            #define MEM_UPDATE(a,b) \
            {{{ \
                (a) = (b); \
                (a ## _peak) = (((a ## _peak) > (a)) ? (a ## _peak) : (a)); \
            }}} /* MEM_UPDATE */
            
            MEM_UPDATE(psbox->stat.mem_info.vsize, proc.vsize);
            MEM_UPDATE(psbox->stat.mem_info.rss, proc.rss * getpagesize());
            psbox->stat.mem_info.minflt = proc.minflt;
            psbox->stat.mem_info.majflt = proc.majflt;
            
            /* Compare memory usage against quota limit, raise out-of-quota 
             * (memory) event when necessary. */
            if (psbox->stat.mem_info.vsize_peak > \
                psbox->task.quota[S_QUOTA_MEMORY])
            {
                DBUG("memory quota exceeded");
                V(&psbox->mutex);
                /* NOTE: unlock the sandbox before posting events */
                POST_EVENT(psbox, _QUOTA, S_QUOTA_MEMORY);
            }
            else
            {
                V(&psbox->mutex);
            }
            
            /* Update the elapsed time stat of sandbox, raise out-of-quota
             * (wallclock) event when occured. */
            if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
            {
                WARN("failed getting wallclock elapsed time");
                /* NOTE: unlock the sandbox before posting events */
                POST_EVENT(psbox, _ERROR, errno, pthread_self());
                goto check_status;
            }
            
            P(&psbox->mutex);
            
            TS_SUBTRACT(ts, psbox->stat.started);
            psbox->stat.elapsed = ts;
            
            /* Compare elapsed time against quota limit, raise out-of-quota 
             * (wallclock) event when necessary. */
            if ((res_t)ts2ms(psbox->stat.elapsed) > 
                psbox->task.quota[S_QUOTA_WALLCLOCK])
            {
                DBUG("wallclock quota exceeded");
                V(&psbox->mutex);
                /* NOTE: unlock the sandbox before posting events */
                POST_EVENT(psbox, _QUOTA, S_QUOTA_WALLCLOCK);
            }
            else
            {
                V(&psbox->mutex);
            }
            
            goto check_status;
        }
        
        if (signo == RT_SIGQUIT)
        {
            /* Do not exit the profiling loop immediately. Instead, go back to 
             * the front to verify if the tracer thread is already finished. */
            DBUG("received quit signal %d", signo);
            goto check_status;
        }
        
        switch (signo)
        {
        case SIGTERM:
        case SIGINT:
        case SIGQUIT:
            /* These signals are redirected to the prisoner process. This allows
             * shell programs running libsandbox to respond to user emitted
             * signals such as <Ctrl> + C (i.e. SIGINT). */
            DBUG("received terminate signal %d", signo);
            kill(-pid, signo);
            break;
        default:
            /* When the process running libsandbox is to be terminated by an 
             * unexpected signal, try to terminate the prisoner process first.
             * But since this is not the fault of the prisoner process, an
             * internal error event is raised. */
            DBUG("received unexpected signal %d", signo);
            POST_EVENT(psbox, _ERROR, EINTR, pthread_self(), signo);
            kill(-pid, SIGKILL);
            break;
        }
        
check_status:
        P(&psbox->mutex);
        continue;
    }
    V(&psbox->mutex);
    
    timer_delete(sw_timer);
    timer_delete(pf_timer);
    
    /* Wait until the the result is decided. In the meanwhile, wake up other 
     * monitors so that they can exit respective monitoring loop as well.  */
    FLUSH_ALL_EVENTS(psbox);
    
    MONITOR_END(psbox);
}

static void *
__sandbox_watcher(sandbox_t * psbox)
{
    MONITOR_BEGIN(psbox);
    
    UPDATE_RESULT(psbox, S_RESULT_PD);
    
    /* Temporary variables */
    ctrl_t * pctrl = &psbox->ctrl;
    proc_t proc = {0};
    proc_bind(psbox, &proc);
    
    /* Wait and handle events */
    P(&pctrl->mutex);
    P(&psbox->mutex);
    while (!HAS_RESULT(psbox))
    {
        /* Exit the watching loop if a) the tracer is already finished, and b) 
         * there are no pending events to be handled */
        if (IS_FINISHED(psbox) && _QUEUE_EMPTY(pctrl))
        {
            /* All (potential) events should have been handled. If the result 
             * is still NOT decided, then it is the fault of the policy. */
            if (!HAS_RESULT(psbox))
            {
                _UPDATE_RESULT(psbox, S_RESULT_BP);
            }
            DBUG("exiting the watching loop");
            break;
        }
        V(&psbox->mutex);
        
        /* An event might have already been posted */
        if (_QUEUE_EMPTY(pctrl))
        {
            pthread_cond_wait(&pctrl->sched, &pctrl->mutex);
            if (_QUEUE_EMPTY(pctrl))
            {
                P(&psbox->mutex);
                continue;
            }
        }
        
        /* Start investigating the event */
        DBUG("detected event: %s {%lu %lu %lu %lu %lu %lu %lu}",
            s_event_type_name(_QUEUE_FRONT(pctrl).type),
            _QUEUE_FRONT(pctrl).data.__bitmap__.A,
            _QUEUE_FRONT(pctrl).data.__bitmap__.B,
            _QUEUE_FRONT(pctrl).data.__bitmap__.C,
            _QUEUE_FRONT(pctrl).data.__bitmap__.D,
            _QUEUE_FRONT(pctrl).data.__bitmap__.E,
            _QUEUE_FRONT(pctrl).data.__bitmap__.F,
            _QUEUE_FRONT(pctrl).data.__bitmap__.G);
        
        /* Consult the sandbox policy to determine next action */
        ((policy_entry_t)pctrl->policy.entry)(&pctrl->policy, \
            &(_QUEUE_FRONT(pctrl)), &pctrl->action);
        
        DBUG("policy decided action: %s {%lu %lu}",
            s_action_type_name(pctrl->action.type),
            pctrl->action.data.__bitmap__.A,
            pctrl->action.data.__bitmap__.B);
        
        /* Perform the decided action */
        P(&psbox->mutex);
        
        /* Drop the obsoleted event and notify other threads about this */
        _QUEUE_POP(pctrl);
        pthread_cond_broadcast(&pctrl->sched);
        
        switch (pctrl->action.type)
        {
        case S_ACTION_CONT:
            break;
        case S_ACTION_FINI:
            /* Terminate the prisoner process */
            trace_kill(&proc, SIGKILL);
            _UPDATE_RESULT(psbox, pctrl->action.data._FINI.result);
            break;
        default:
        case S_ACTION_KILL:
            /* Terminate the prisoner process */
            trace_kill(&proc, SIGKILL);
            _UPDATE_RESULT(psbox, pctrl->action.data._KILL.result);
            break;
        }
    }
    V(&psbox->mutex);
    V(&pctrl->mutex);
    
    /* Wake up other monitors and let them know that the result is already 
     * decided, so that they can exit respective monitoring loop as well.  */
    FLUSH_ALL_EVENTS(psbox);
    
    MONITOR_END(psbox);
}

static void *
__sandbox_tracer(sandbox_t * psbox)
{
    MONITOR_BEGIN(psbox);
    
    UPDATE_STATUS(psbox, S_STATUS_EXE);
    
    /* Temporary variables. */
    pid_t self = getpid();
    pid_t pid = psbox->ctrl.pid;
    siginfo_t w_info;
    int w_opt = WEXITED | WSTOPPED;
    int w_res = 0;
    proc_t proc = {0};
    proc_bind(psbox, &proc);
    long sc_stack[8] = {0};
    int sc_top = 0;
    
    /* Make an initial wait to verify the first system call. */
    if ((w_res = waitid(P_PID, pid, &w_info, w_opt)) < 0)
    {
        WARN("waitid");
        POST_EVENT(psbox, _ERROR, errno, pthread_self());
        kill(-pid, SIGKILL);
    }
    
    /* Clear cache in order to increase timing accuracy */
    cache_flush();
    
    /* Entering the tracing loop */
    do
    {
        /* Trace state refresh of the prisoner process */
        UPDATE_STATUS(psbox, S_STATUS_BLK);

        /* Obtain signal info of prisoner process */
        if (!proc_probe(pid, PROBE_SIGINFO, &proc))
        {
            WARN("failed to probe process: %d", pid);
            kill(-pid, SIGKILL);
        }
        
        /* Notify the profiler thread to examine stat */
        if (kill(self, RT_SIGSTAT) != 0)
        {
            WARN("failed to notify the profiler thread");
            kill(-pid, SIGKILL);
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
                    goto unknown_signal;
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
                break;
            case SIGTRAP:
                /* Collect additional info of prisoner process */
                if (!proc_probe(pid, PROBE_REGS | PROBE_OP, &proc))
                {
                    WARN("failed to probe process: %d", pid);
                    kill(-pid, SIGKILL);
                    break;
                }
                /* Inspect opcode and tracing flags to see if the SIGTRAP
                 * signal was due to a system call invocation or return. The 
                 * subtleties are wrapped in the IS_SYSCALL and IS_SYSRET 
                 * macros defined in <platform.h> */
                if (IS_SYSCALL(&proc) || IS_SYSRET(&proc))
                {
                    long sc = THE_SYSCALL(&proc);
                    
                    P(&psbox->mutex);
                    psbox->stat.syscall = sc;
                    V(&psbox->mutex);
                    
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
                        goto unknown_signal;
                    }
                    DBUG("detected post-execve SIGTRAP");
                }
#ifdef WITH_SOFTWARE_TSC
#warning "software tsc is an experimental feature"
                /* Update the tsc instructions counter */
                P(&psbox->mutex);
                psbox->stat.cpu_info.tsc++;
                DBUG("tsc                         %010llu", \
                    psbox->stat.cpu_info.tsc);
                V(&psbox->mutex);
#endif /* WITH_SOFTWARE_TSC */
                break;
            default:            /* Other runtime errors */
                goto unknown_signal;
            }
        } /* trapped */
        else if ((w_info.si_code == CLD_STOPPED) || \
                 (w_info.si_code == CLD_KILLED) || \
                 (w_info.si_code == CLD_DUMPED))
        {
            DBUG("wait: signaled (%d)", w_info.si_status);
    unknown_signal:
            P(&psbox->mutex);
            psbox->stat.signal.signo = w_info.si_status;
            psbox->stat.signal.code = proc.siginfo.si_code;
            V(&psbox->mutex);
            /* NOTE: unlock the sandbox before posting events */
            POST_EVENT(psbox, _SIGNAL, w_info.si_status, proc.siginfo.si_code);
        }
        else if (w_info.si_code == CLD_EXITED)
        {
            DBUG("wait: exited (%d)", w_info.si_status);
            P(&psbox->mutex);
            psbox->stat.exitcode = w_info.si_status;
            V(&psbox->mutex);
            /* NOTE: unlock the sandbox before posting events */
            POST_EVENT(psbox, _EXIT, w_info.si_status);
        }
        else
        {
            DBUG("wait: unknown (si_code = %d)", w_info.si_code);
            /* unknown event, should not reach here! */
        }
        
        /* Wait for the watcher to handle all pending events. This is also the 
         * point for user-specified policy modules to dump / visit the address 
         * space of the prisoner process using proc_dump(). If the result is 
         * already decided, this will also exit the tracing loop. */
        WAIT4_EVENT_HANDLING(psbox, &proc);
        
        /* Hack the prisoner process to ensure safe tracing. This includes 
         * modifying some system call parameters / return values and flipping 
         * tracing flags of the proc structure defined in <platform.h>. */
        if (!trace_hack(&proc))
        {
            WARN("failed hacking the prisoner process");
            kill(-pid, SIGKILL);
            /* Do NOT post event here as the prisoner process may have gone. */
        }
        
        /* Schedule for next trace */
#ifdef WITH_SOFTWARE_TSC
        if (!trace_next(&proc, TRACE_SINGLE_STEP))
#else /* WITHOUT_SOFTWARE_TSC */
        if (!trace_next(&proc, TRACE_SYSTEM_CALL))
#endif /* WITH_SOFTWARE_TSC */
        {
            WARN("failed scheduling next trace");
            kill(-pid, SIGKILL);
            /* Do NOT post event here as the prisoner process may have gone. */
        }
        
        /* Wait until the prisoner process is trapped  */
        UPDATE_STATUS(psbox, S_STATUS_EXE);
        DBUG("---------------------------------------------------------------");
        DBUG("waitid(%d,%d,%p,%d)", P_PID, pid, &w_info, w_opt);
    } while ((w_res = waitid(P_PID, pid, &w_info, w_opt)) >= 0);
    
    UPDATE_STATUS(psbox, S_STATUS_FIN);
    
    /* Wait until the the result is decided. In the meanwhile, wake up other 
     * monitors so that they can exit respective monitoring loop as well.  */
    FLUSH_ALL_EVENTS(psbox);
    
    /* Final notification for monitors to exit respective monitoring loops. */
    kill(self, RT_SIGQUIT);
    
    /* Notify trace_main() to return */
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
        if ((unsigned)pevent->data._SYSCALL.scinfo >= MAKE_WORD(0, SCMODE_MAX))
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
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_RT}}};
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

#ifdef __cplusplus
} /* extern "C" */
#endif
