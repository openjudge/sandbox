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
/** 
 * @file internal.h 
 * @brief Routines for manipulating sandbox internals.
 */
#ifndef __OJS_INTERNAL_H__
#define __OJS_INTERNAL_H__

#include <sandbox.h>

#include <assert.h>             /* assert() */
#include <errno.h>              /* errno, strerror() */
#include <pthread.h>            /* pthread_mutex_{lock,unlock}() */
#include <signal.h>             /* kill(), SIG* */
#include <stdio.h>              /* fprintf(), fflush(), stderr */
#include <time.h>               /* struct timespec */

#ifdef __cplusplus
extern "C"
{
#endif

/* Macros for composing conditional rvalue */
#define RVAL_IF(x)              ((x) ? (
#define RVAL_ELSE               ) : (
#define RVAL_FI                 ))

/* Macros for producing debugging messages */
#ifndef _LOG
#ifdef NDEBUG
#define _LOG(fmt,x...) ((void)0)
#else
#define _LOG(fmt,x...) \
{{{ \
    int errnum = errno; \
    fprintf(stderr, fmt "\n", ##x); \
    fflush(stderr); \
    errno = errnum; \
}}}
#endif /* NDEBUG */
#endif /* _LOG */

#ifndef DBUG
#ifdef NDEBUG
#define DBUG(fmt,x...) ((void)0)
#else
#ifndef DBUG_PREFIX
#define DBUG_PREFIX "debug> "
#endif /* DBUG_PREFIX */
#define DBUG(fmt,x...) \
{{{ \
    _LOG(DBUG_PREFIX fmt, ##x); \
}}}
#endif /* NDEBUG */
#endif /* DBUG */

#ifndef WARN
#ifdef NDEBUG
#define WARN(fmt,x...) ((void)0)
#else
#ifndef WARN_PREFIX
#define WARN_PREFIX DBUG_PREFIX "[WARNING] "
#endif /* WARN_PREFIX */
#define WARN(fmt,x...) \
{{{ \
    _LOG(WARN_PREFIX fmt ": %d (%s)", ##x, errno, strerror(errno)); \
}}}
#endif /* NDEBUG */
#endif /* WARN */

#ifndef FUNC_BEGIN
#define FUNC_BEGIN(fmt,x...) \
{{{ \
    DBUG("calling: %s(" fmt ")", __FUNCTION__, ##x); \
}}}
#endif /* FUNC_BEGIN */

#ifndef FUNC_RET
#define _RET_1(r) \
{{{ \
    DBUG("leaving: %s() = <opaque>", __FUNCTION__); \
    return (r); \
}}}
#define _RET_2(fmt,r) \
{{{ \
    DBUG("leaving: %s() = " fmt, __FUNCTION__, (r)); \
    return (r); \
}}}
#define _RET_X(r,fmt,x...) \
{{{ \
    DBUG("leaving: %s() = " fmt, __FUNCTION__, ##x); \
    return (r); \
}}}
#define _7TH(arg1,arg2,arg3,arg4,arg5,arg6,arg7,...) \
    arg7
#define _RET(...) \
    _7TH(__VA_ARGS__, _RET_X, _RET_X, _RET_X, _RET_X, _RET_2, _RET_1, )
#define FUNC_RET(...) \
    _RET(__VA_ARGS__)(__VA_ARGS__)
#endif /* FUNC_RET */

#ifndef PROC_BEGIN
#define PROC_BEGIN FUNC_BEGIN
#endif /* PROC_BEGIN */

#ifndef PROC_END
#define PROC_END(void) \
{{{ \
    DBUG("leaving: %s()", __FUNCTION__); \
    return; \
}}}
#endif /* PROC_END */

/* Macros for concurrency control */
#define LOCK_INITIALIZER (lock_t){PTHREAD_MUTEX_INITIALIZER, \
    PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, false}

#ifndef P
#define P(pm) \
{{{ \
    if (pthread_mutex_lock((pm)) != 0) \
    { \
        WARN("mutex lock failed"); \
    } \
}}}
#endif /* P */

#ifndef V
#define V(pm) \
{{{ \
    if (pthread_mutex_unlock((pm)) != 0) \
    { \
        WARN("mutex unlock failed"); \
    } \
}}}
#endif /* V */

#ifndef __RWLOCK_WRITE_OPTIMIZED
#define __RWLOCK_WRITE_OPTIMIZED (false)
#endif /* __RWLOCK_WRITE_OPTIMIZED */

#ifndef __RWLOCK_READER_WAIT
#define __RWLOCK_READER_WAIT(plock, cond) \
{{{ \
    while ((((plock)->wrlock) && (__RWLOCK_WRITE_OPTIMIZED)) || (cond)) \
    { \
        DBUG("%s() waiting for shared lock (%dr/%dw)", __FUNCTION__, \
            ((plock)->rdcount), (((plock)->wrlock) ? 1 : 0)); \
        pthread_cond_wait(&((plock)->rdc), &((plock)->mutex)); \
    } \
}}} 
#endif /* __RWLOCK_READER_WAIT */

#ifndef __RWLOCK_WRITER_WAIT
#define __RWLOCK_WRITER_WAIT(plock,cond) \
{{{ \
    while (((plock)->wrlock) || (((plock)->rdcount) > 0) || (cond)) \
    { \
        DBUG("%s() waiting for exclusive lock (%dr/%dw)", __FUNCTION__, \
            ((plock)->rdcount), (((plock)->wrlock) ? 1 : 0)); \
        pthread_cond_wait(&((plock)->wrc), &((plock)->mutex)); \
    } \
}}}
#endif /* __RWLOCK_WRITER_WAIT */

#ifndef __RWLOCK_LOCK_SH_WHEN
#define __RWLOCK_LOCK_SH_WHEN(plock,cond) \
{{{ \
    __RWLOCK_READER_WAIT(plock, !(cond)); \
    ++((plock)->rdcount); \
    DBUG("%s() acquired lock (shared)", __FUNCTION__); \
}}}
#endif /* __RWLOCK_LOCK_SH_WHEN */

#ifndef __RWLOCK_LOCK_EX_WHEN
#define __RWLOCK_LOCK_EX_WHEN(plock,cond) \
{{{ \
    __RWLOCK_WRITER_WAIT(plock, !(cond)); \
    ((plock)->wrlock) = true; \
    DBUG("%s() acquired lock (exclusive)", __FUNCTION__); \
}}}
#endif /* __RWLOCK_LOCK_EX_WHEN */

#ifndef __RWLOCK_UNLOCK
#define __RWLOCK_UNLOCK(plock) \
{{{ \
    assert((((plock)->rdcount) > 0) || ((plock)->wrlock)); \
    if (((plock)->rdcount) > 0) \
    { \
        --((plock)->rdcount); \
    } \
    else \
    { \
        ((plock)->wrlock) = false; \
    } \
    DBUG("%s() released lock", __FUNCTION__); \
}}}
#endif /* __RWLOCK_UNLOCK */

#ifndef LOCK_ON_COND
#define LOCK_ON_COND(psbox,mode,cond) \
{{{ \
    P(&((psbox)->lock.mutex)); \
    __RWLOCK_LOCK_ ## mode ## _WHEN(&((psbox)->lock), (cond)); \
    V(&((psbox)->lock.mutex)); \
}}}
#endif /* LOCK_ON_COND */

#ifndef LOCK
#define LOCK(psbox,mode) \
{{{ \
    LOCK_ON_COND(psbox, mode, true); \
}}}
#endif /* LOCK */

#ifndef UNLOCK
#define UNLOCK(psbox) \
{{{ \
    P(&((psbox)->lock.mutex)); \
    __RWLOCK_UNLOCK(&((psbox)->lock)); \
    pthread_cond_broadcast(&((psbox)->lock.rdc)); \
    if (((psbox)->lock.rdcount) == 0) \
    { \
        pthread_cond_broadcast(&((psbox)->lock.wrc)); \
    } \
    V(&((psbox)->lock.mutex)); \
}}}
#endif /* UNLOCK */

#ifndef RELOCK
#define RELOCK(psbox,mode) \
{{{ \
    P(&((psbox)->lock.mutex)); \
    __RWLOCK_UNLOCK(&((psbox)->lock)); \
    __RWLOCK_LOCK_ ## mode ## _WHEN(&((psbox)->lock), true); \
    V(&((psbox)->lock.mutex)); \
}}}
#endif /* RELOCK */

/* Macros for sandbox coordination */
#define SIGEXIT          (SIGUSR1)
#define SIGSTAT          (SIGUSR2)

#define PROF_FREQ        (100)
#define STAT_FREQ        (5)

/* Macros for testing sandbox status */
#ifndef NOT_STARTED
#define NOT_STARTED(psbox) \
    ((((psbox)->status) == S_STATUS_RDY) || \
     (((psbox)->status) == S_STATUS_PRE))
#endif /* NOT_STARTED */

#ifndef IS_RUNNING
#define IS_RUNNING(psbox) \
    (((psbox)->status) == S_STATUS_EXE)
#endif /* IS_RUNNING */

#ifndef IS_BLOCKED
#define IS_BLOCKED(psbox) \
    (((psbox)->status) == S_STATUS_BLK)
#endif /* IS_BLOCKED */

#ifndef IS_FINISHED
#define IS_FINISHED(psbox) \
    (((psbox)->status) == S_STATUS_FIN)
#endif /* IS_FINISHED */

#ifndef HAS_RESULT
#define HAS_RESULT(psbox) \
    (((psbox)->result) != S_RESULT_PD)
#endif /* HAS_RESULT */

/* Macros for new monitors */
#define MONITOR_BEGIN(psbox) \
{{{ \
    FUNC_BEGIN("%p", psbox); \
    assert(psbox); \
    if (psbox == NULL) \
    { \
        FUNC_RET("%p", (void *)NULL); \
    } \
    LOCK_ON_COND(psbox, SH, !NOT_STARTED(psbox) || HAS_RESULT(psbox)); \
    if (NOT_STARTED(psbox) || IS_FINISHED(psbox) || HAS_RESULT(psbox)) \
    { \
        UNLOCK(psbox); \
        FUNC_RET("%p", (void *)&((psbox)->result)); \
    } \
    else \
    { \
        UNLOCK(psbox); \
    } \
}}} /* MONITOR_BEGIN */

#define MONITOR_END(psbox) \
{{{ \
    FUNC_RET("%p", (void *)&((psbox)->result)); \
}}} /* MONITOR_END */

/* Macros for manipulating struct timespec from <time.h> */
#define ms2ns(x) (1000000 * (x))
#define ts2ms(x) ((((x).tv_sec) * 1000) + (((x).tv_nsec) / 1000000))
#define fts2ms(x) ((0.000001 * ((x).tv_nsec)) + (1000.0 * ((x).tv_sec)))

#define TS_LESS(x,y) \
    RVAL_IF((x).tv_sec < (y).tv_sec) \
        true \
    RVAL_ELSE \
        RVAL_IF(((x).tv_sec == (y).tv_sec) && ((x).tv_nsec < (y).tv_nsec)) \
            true \
        RVAL_ELSE \
            false \
        RVAL_FI \
    RVAL_FI \
/* TS_LESS */

#define TS_UPDATE(a,x) \
    RVAL_IF(TS_LESS(a,x)) \
        (a) = (x) \
    RVAL_ELSE \
        (a) \
    RVAL_FI \
/* TS_UPDATE */

#define TS_INPLACE_ADD(x,y) \
{{{ \
    (x).tv_sec += (y).tv_sec; \
    (x).tv_nsec += (y).tv_nsec; \
    while ((x).tv_nsec >= ms2ns(1000)) \
    { \
        ++(x).tv_sec; \
        (x).tv_nsec -= ms2ns(1000); \
    } \
}}} /* TS_INPLACE_ADD */

#define TS_INPLACE_SUB(x,y) \
{{{ \
    (x).tv_sec -= (y).tv_sec; \
    (x).tv_nsec -= (y).tv_nsec; \
    while ((x).tv_nsec < 0) \
    { \
        --(x).tv_sec; \
        (x).tv_nsec += ms2ns(1000); \
    } \
}}} /* TS_INPLACE_SUB */

/**
 * @param[in] type any of the constant values defined in \c event_type_t
 * @return a statically allocated string name for the specified event type. 
 */
const char * s_event_type_name(int type);

/**
 * @param[in] type any of the constant values defined in \c action_type_t
 * @return a statically allocated string name for the specified action type. 
 */
const char * s_action_type_name(int type);

/**
 * @param[in] status any of the constant values defined in \c status_t
 * @return a statically allocated string name for the specified status. 
 */
const char * s_status_name(int status);

/**
 * @param[in] result any of the constant values defined in \c result_t
 * @return a statically allocated string name for the specified result. 
 */
const char * s_result_name(int result);

/**
 * @param[in] option any of the constant values defined in \c option_t
 * @return a statically allocated string name for the specified option.
 */
const char * s_trace_opt_name(int option);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_INTERNAL_H__ */
