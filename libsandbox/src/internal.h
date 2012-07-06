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

/* Macros for producing debugging messages */
#ifndef _LOG
#ifdef NDEBUG
#define _LOG(fmt,x...) ((void)0)
#else
#define _LOG(fmt,x...) \
{{{ \
    fprintf(stderr, fmt "\n", ##x); \
    fflush(stderr); \
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
#define FUNC_RET(fmt,r) \
{{{ \
    DBUG("leaving: %s() = " fmt, __FUNCTION__, (r)); \
    return (r); \
}}}
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

/* Macros for pthread mutex lock / unlock */
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

/* Macros for testing sandbox status */
#ifndef NOT_STARTED
#define NOT_STARTED(psbox) \
    ((((psbox)->status) == S_STATUS_RDY) || (((psbox)->status) == S_STATUS_PRE))
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
        FUNC_RET("%p", NULL); \
    } \
    P(&((psbox)->mutex)); \
    while (NOT_STARTED(psbox)) \
    { \
        DBUG("waiting for the sandbox to start"); \
        pthread_cond_wait(&((psbox)->update), &((psbox)->mutex)); \
    } \
    if (NOT_STARTED(psbox) || IS_FINISHED(psbox)) \
    { \
        V(&((psbox)->mutex)); \
        FUNC_RET("%p", (void *)&((psbox)->result)); \
    } \
    V(&((psbox)->mutex)); \
}}} /* MONITOR_BEGIN */

#define MONITOR_END(psbox) \
{{{ \
    FUNC_RET("%p", (void *)&((psbox)->result)); \
}}} /* MONITOR_END */

/* Macros for manipulating struct timespec from <time.h> */
#define ms2ns(x) (1000000 * (x))
#define ts2ms(x) ((((x).tv_sec) * 1000) + (((x).tv_nsec) / 1000000))
#define TS_SUBTRACT(x,y) \
{{{ \
    if ((x).tv_nsec < (y).tv_nsec) \
    { \
        long sec = 1 + ((y).tv_nsec - (x).tv_nsec) / ms2ns(1000); \
        (x).tv_sec -= sec; \
        (x).tv_nsec += ms2ns(1000 * sec); \
    } \
    if ((x).tv_nsec - (y).tv_nsec >= ms2ns(1000)) \
    { \
        long sec = ((y).tv_nsec - (x).tv_nsec) / ms2ns(1000); \
        (x).tv_nsec -= ms2ns(1000 * sec); \
        (x).tv_sec += sec; \
    } \
    (x).tv_sec -= (y).tv_sec; \
    (x).tv_nsec -= (y).tv_nsec; \
}}} /* TS_SUBTRACT */

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
const char * t_option_name(int option);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_INTERNAL_H__ */
