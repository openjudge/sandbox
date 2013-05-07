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
 * @file sandbox.h
 * @brief Methods for creating and manipulating a sandbox object.
 */
#ifndef __OJS_SANDBOX_H__
#define __OJS_SANDBOX_H__

#include <limits.h>             /* PATH_MAX */
#include <pthread.h>            /* pthread_mutex_t, pthread_cond_t */
#include <stdbool.h>            /* false, true */
#include <sys/resource.h>       /* rlim_t, RLIM_INFINITY */
#include <sys/types.h>          /* uid_t, gid_t */
#include <time.h>               /* struct timespec */

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef FILENO_MAX
#define FILENO_MAX 256
#endif /* FILENO_MAX */

#ifndef SBOX_ARG_MAX
#if defined(ARG_MAX) && (ARG_MAX > 65536)
#define SBOX_ARG_MAX ARG_MAX
#warning "overriding default value of SBOX_ARG_MAX"
#else
#define SBOX_ARG_MAX 65536
#endif
#endif /* SBOX_ARG_MAX */

#ifndef SBOX_PATH_MAX
#if defined(PATH_MAX) && (PATH_MAX > 4096)
#define SBOX_PATH_MAX PATH_MAX
#warning "overriding default value of SBOX_PATH_MAX"
#else
#define SBOX_PATH_MAX 4096
#endif
#endif /* SBOX_PATH_MAX */

#ifndef SBOX_QUOTA_INF
#define SBOX_QUOTA_INF RLIM_INFINITY
#endif /* SBOX_RES_INF */

#ifndef RES_INFINITY
#define RES_INFINITY SBOX_QUOTA_INF
#endif /* RES_INFINITY */

/* Maximum number of monitor threads */
#ifndef SBOX_MONITOR_MAX
#define SBOX_MONITOR_MAX        8
#else
#warning "overriding default monitor pool size"
#endif /* SBOX_MONITOR_MAX */

/* Maximum number of queued events */
#ifndef SBOX_EVENT_MAX
#define SBOX_EVENT_MAX          32
#else
#warning "overriding default event queue size"
#endif /* SBOX_EVENT_MAX */

/**
 * @brief Serialized representation of a command and its arguments.
 */
typedef struct
{
    char buff[SBOX_ARG_MAX];    /**< command line buffer */
    int args[SBOX_ARG_MAX];     /**< arguments offset */
} command_t;

/**
 * @brief Task quota type.
 */
typedef enum
{
    S_QUOTA_WALLCLOCK  = 0,     /*!< Wall clock time (msec) */
    S_QUOTA_CPU        = 1,     /*!< CPU usage usr + sys (msec) */
    S_QUOTA_MEMORY     = 2,     /*!< Virtual memory size (bytes) */
    S_QUOTA_DISK       = 3,     /*!< Maximum output file size (bytes) */
} quota_type_t;

#define QUOTA_TOTAL 4

/**
 * @brief Quota size counter.
 */
typedef rlim_t res_t;

/**
 * @brief Static specification of a task.
 */
typedef struct
{
    command_t comm;             /**< command (with arguments) to be executed */
    char jail[SBOX_PATH_MAX];   /**< chroot to this path before running cmd */
    uid_t uid;                  /**< run command as this user */
    gid_t gid;                  /**< run command as member of this group */
    int ifd;                    /**< file descriptor for task input */
    int ofd;                    /**< file descriptor for task output */
    int efd;                    /**< file descriptor for task error log */
    res_t quota[QUOTA_TOTAL];   /**< block the task program if quota exceeds */
} task_t;

#ifndef HAVE_SYSCALL_T
#ifdef __x86_64__
typedef struct
{
    int scno;
    int mode;
} syscall_t;
#else
typedef long syscall_t;
#endif /* __x86_64 */
#endif /* HAVE_SYSCALL_T */

#ifndef HAVE_SIGNAL_T
typedef struct
{
    int signo;
    int code;
} signal_t;
#endif /* HAVE_SIGNAL_T */

/**
 * @brief Runtime / cumulative information about a task.
 */
typedef struct
{
    struct timespec started;    /**< start time since Epoch */
    struct timespec elapsed;    /**< elapsed wallclock time since started */
    struct
    {
        struct timespec clock;  /**< cpu clock time usage */
        struct timespec utime;  /**< cpu usage in user mode */
        struct timespec stime;  /**< cpu usage in kernel mode */
        unsigned long long tsc; /**< software time-stamp counter */
    } cpu_info;                 /**< collection of cpu usage stat */
    struct
    {
        res_t vsize;            /**< virtual memory usage (bytes) */
        res_t vsize_peak;       /**< virtual memory peak usage (bytes) */
        res_t rss;              /**< resident set size (bytes) */
        res_t rss_peak;         /**< resident set peak size (bytes) */
        res_t minflt;           /**< minor page faults (# of pages) */
        res_t majflt;           /**< major page faults (# of pages) */
    } mem_info;                 /**< collection of memory usage stat */
    long syscall;               /**< last / current syscall info */
    signal_t signal;            /**< last / current signal info */
    int exitcode;               /**< exit code */
} stat_t;

/**
 * @brief Types of sandbox execution status.
 */
typedef enum
{
    S_STATUS_PRE       = 0,     /*!< Preparing (not ready to execute) */
    S_STATUS_RDY       = 1,     /*!< Ready (waiting for execution) */
    S_STATUS_EXE       = 2,     /*!< Executing (waiting for event) */
    S_STATUS_BLK       = 3,     /*!< Blocked (handling event) */
    S_STATUS_FIN       = 4,     /*!< Finished */
} status_t;

/**
 * @brief Types of sandbox execution result.
 */
typedef enum
{
    S_RESULT_PD        =  0,    /*!< Pending */
    S_RESULT_OK        =  1,    /*!< Okay */
    S_RESULT_RF        =  2,    /*!< Restricted Function */
    S_RESULT_ML        =  3,    /*!< Memory Limit Exceed */
    S_RESULT_OL        =  4,    /*!< Output Limit Exceed */
    S_RESULT_TL        =  5,    /*!< Time Limit Exceed */
    S_RESULT_RT        =  6,    /*!< Run Time Error (SIGSEGV, SIGFPE, ...) */
    S_RESULT_AT        =  7,    /*!< Abnormal Termination */
    S_RESULT_IE        =  8,    /*!< Internal Error (of sandbox executor) */
    S_RESULT_BP        =  9,    /*!< Bad Policy (since 0.3.3) */
    S_RESULT_R0        = 10,    /*!< Reserved result type 0 (since 0.3.2) */
    S_RESULT_R1        = 11,    /*!< Reserved result type 1 (since 0.3.2) */
    S_RESULT_R2        = 12,    /*!< Reserved result type 2 (since 0.3.2) */
    S_RESULT_R3        = 13,    /*!< Reserved result type 3 (since 0.3.2) */
    S_RESULT_R4        = 14,    /*!< Reserved result type 4 (since 0.3.2) */
    S_RESULT_R5        = 15,    /*!< Reserved result type 5 (since 0.3.2) */
} result_t;

/**
 * @brief Types of sandbox internal events.
 */
typedef enum
{
    S_EVENT_ERROR      = 0,     /*!< sandbox encountered internal error */
    S_EVENT_EXIT       = 1,     /*!< target program exited */
    S_EVENT_SIGNAL     = 2,     /*!< target program was signaled unexpectedly */
    S_EVENT_SYSCALL    = 3,     /*!< target program issued a system call */
    S_EVENT_SYSRET     = 4,     /*!< target program returned from system call */
    S_EVENT_QUOTA      = 5,     /*!< target program exceeds resource quota */
} event_type_t;

/**
 * @brief Event specific data bindings.
 */
typedef union
{
    /* The *__bitmap__* field does not have corresponding event type, it is
     * there to ensure that an instance of this union can be properly
     * instantiated with braced initializer. */
    struct
    {
        unsigned long A, B, C, D, E, F, G;
    } __bitmap__;
    struct
    {
        long code;
        long origin;
        long data;
    } _ERROR;
    struct
    {
        long code;
    } _EXIT;
    struct
    {
        long signo;
        long code;
    } _SIGNAL;
    struct
    {
        long scinfo;
        long a, b, c, d, e, f;
    } _SYSCALL;
    struct
    {
        long scinfo;
        long retval;
    } _SYSRET;
    struct
    {
        long type;
    } _QUOTA;
} event_data_t;

/**
 * @brief Structure for holding events and event-associated data.
 */
typedef struct
{
    event_type_t type;          /**< event type */
    event_data_t data;          /**< event data */
} event_t;

/**
 * @brief Types of sandbox actions.
 */
typedef enum
{
    S_ACTION_CONT      = 0,     /*!< Continute */
    S_ACTION_FINI      = 1,     /*!< Finish (safe exit) */
    S_ACTION_KILL      = 2,     /*!< Kill (instant exit) */
    /* TODO identify other action types */
} action_type_t;

/**
 * @brief Action specific data bindings.
 */
typedef union
{
    struct
    {
        unsigned long A, B;
    } __bitmap__;
    struct
    {
        int reserved;
    } _CONT;
    struct
    {
        int result;
    } _FINI;
    struct
    {
        int result;
    } _KILL;
    /* TODO add action-specific data structures */
} action_data_t;

/**
 * @brief Structure for holding actions and action-associated data.
 */
typedef struct
{
    action_type_t type;         /**< action type */
    action_data_t data;         /**< action data */
} action_t;

/**
 * @brief Structure for sandbox policy object entry and private data.
 *
 * The design of *policy_t* is to facilitate high-level language extension.
 *
 * For example, if we are to extend / wrap *libsandbox* with Python, it is
 * essential to be able to compile the real policies as Python modules or even
 * more complex binary objects, and *libsandbox* must be able to invoke real
 * policy objects written in high-level languages via a unified interface.
 *
 * In our design, the real policy object (address to the Python object) can be
 * hooked at the *data* field of the *policy_t* object, and a corresponding
 * *policy_entry_t* function is to be hooked at the *entry* field. In this
 * manner, Python developers can design the real policy objects anyway they
 * desire. And they only need to ensure that the *policy_entry_t* function knows
 * how to invoke real policy objects properly.
 */
typedef struct
{
    void * entry;               /**< policy object entry point */
    long data;                  /**< reserved data / control storage */
} policy_t;

/**
 * @brief Entry function signature of sandbox policy object.
 */
typedef void (* policy_entry_t)(const policy_t *, const event_t *, action_t *);

#ifndef thread_func_t
/**
 * @brief Entry function signature of sandbox monitor object.
 */
typedef void * (* thread_func_t)(void *);
#endif /* thread_func_t */

/**
 * @brief Structure for sandbox tracer and monitor objects.
 */
typedef struct
{
    thread_func_t target;       /**< thread function to be started */
    pthread_t tid;              /**< thread id of the worker when started */
} worker_t;

/**
 * @brief Configurable controller of a sandbox object.
 */
typedef struct
{
    pid_t pid;                  /**< id of the process being traced */
    action_t action;            /**< the action to be suggested by the policy */
    policy_t policy;            /**< the policy to consult for actions */
    worker_t tracer;            /**< the main tracer thread */
    worker_t monitor[SBOX_MONITOR_MAX]; /**< the pool of monitor threads */
    struct
    {
        int head;
        int size;
        event_t list[SBOX_EVENT_MAX];
    } event;                    /**< the queue of pending events */
} ctrl_t;

/**
 * @brief Read-write lock of a sandbox object.
 */
typedef struct
{
    pthread_mutex_t mutex;      /**< POSIX mutex */
    pthread_cond_t rdc;         /**< eligible for read condition */
    pthread_cond_t wrc;         /**< eligible for write condition */
    int rdcount;                /**< number of concurrent readers */
    bool wrlock;                /**< write locked */
} lock_t;

/**
 * @brief Structure for collecting everything needed to run a sandbox.
 */
typedef struct
{
    status_t status;            /**< task status */
    result_t result;            /**< task result */
    task_t task;                /**< task initial specification */
    stat_t stat;                /**< task cumulative statistics */
    ctrl_t ctrl;                /**< configurable sandbox controller */
    lock_t lock;                /**< rwlock for concurrency control */
} sandbox_t;

/**
 * @brief Initialize a \c sandbox_t object.
 * @param[in,out] psbox pointer to the \c sandbox_t object to be initialized
 * @param[in] argv command line argument array of the targeted program
 * @return 0 on success
 */
int sandbox_init(sandbox_t * psbox, const char * argv[]);

/**
 * @brief Destroy a \c sandbox_t object.
 * @param[in,out] psbox pointer to the \c sandbox_t object to be destroied
 * @return 0 on success
 */
int sandbox_fini(sandbox_t * psbox);

/**
 * @brief Check if sandbox is ready to (re)start, update status accordingly.
 * @param[in,out] psbox pointer to the \c sandbox_t object to be checked
 * @return true on success
 * In order to restart a finished sandbox, the caller of this function must
 *   a) properly stop previous monitor (i.e. unlock the mutex), and
 *   b) rewind / reopen / reset I/O channels of task specification
 */
bool sandbox_check(sandbox_t * psbox);

/**
 * @brief Start executing the task binded with the sandbox.
 * @param[in,out] psbox pointer to the \c sandbox_t object to be started
 * @return pointer to the \c result field of \c psbox, or NULL on failure
 */
result_t * sandbox_execute(sandbox_t * psbox);

/**
 * @brief Default policy with a baseline (black) list of system calls.
 * @param[in] ppolicy pointer to the \c policy_t object of the sandbox, or NULL
 * @param[in] pevent pointer to the builtin \c event_t object of the sandbox
 * @param[out] paction pointer to the builtin \c action_t object of the sandbox
 */
void sandbox_default_policy(const policy_t * ppolicy, const event_t * pevent,
    action_t * paction);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_SANDBOX_H__ */

