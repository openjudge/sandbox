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
/** 
 * @file platform.h 
 * @brief Platform dependent features wrapper.
 */
#ifndef __OJS_PLATFORM_H__
#define __OJS_PLATFORM_H__

#include <pthread.h>            /* pthread_t */
#include <signal.h>             /* siginfo_t, SIGTRAP */
#include <time.h>               /* struct timespec */
#include <sys/types.h>          /* pid_t */
#ifdef __linux__
#include <sys/reg.h>            /* EAX, EBX, ... */
#include <sys/syscall.h>        /* SYS_execve, ... */
#include <sys/user.h>           /* struct user_regs_struct */
#endif /* __linux__ */

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __cplusplus
#ifndef HAVE_BOOL
#define HAVE_BOOL
/** 
 * @brief Emulation of the C++ bool type. 
 */
typedef enum 
{
    false,                      /*!< false */
    true                        /*!< true */
} bool;
#endif /* HAVE_BOOL */
#endif /* __cplusplus */

/* architecture detection */
#undef SUPPORTED_ARCH

#ifdef __x86_64__
#define SUPPORTED_ARCH
#ifndef __WORDSIZE
#define __WORDSIZE              64
#endif /* __WORDSIZE */
#define MAKE_WORD(a,b) \
    ((long)(((int)(a)) | (((long)((int)(b))) << 32))) \
/* MAKE_WORD */
#define OPCODE(op) \
    ((unsigned long)(op) & (~((unsigned long)(~0LU << 16)))) \
/* OPCODE */
#define OP_INT80                0x80cd
#define OP_SYSCALL              0x050f
#define OP_SYSENTER             0x340f
#define NAX                     RAX
#define ORIG_NAX                ORIG_RAX
#endif /* __x86_64__ */

#ifdef __i386__
#define SUPPORTED_ARCH
#ifndef __WORDSIZE
#define __WORDSIZE              32
#endif /* __WORDSIZE */
#define MAKE_WORD(a,b) \
    ((long)(((short)(a)) | (((long)((short)(b))) << 16))) \
/* MAKE_WORD */
#define OPCODE(op) \
    ((unsigned long)(op) & (~((unsigned long)(~0LU << 16)))) \
/* OPCODE */
#define OP_INT80                0x80cd
#define OP_SYSENTER             0x340f
#define NAX                     EAX
#define ORIG_NAX                ORIG_EAX
#endif /* __i386__ */

#ifndef SUPPORTED_ARCH
#error "this architecture is not supported"
#endif /* SUPPORTED_ARCH */

/* os detection */
#undef SUPPORTED_OS

#ifdef __linux__
#define SUPPORTED_OS
#endif /* __linux__ */

#ifndef SUPPORTED_OS
#error "this system is not supported"
#endif /* SUPPORTED_OS */

/** 
 * @brief Structure for collecting process runtime information. 
 */
typedef struct
{
    pid_t pid;                  /**< process id */
    pid_t ppid;                 /**< parent process id */
    char state;                 /**< state of the process */
    unsigned long flags;        /**< unknown */
    struct timespec utime;      /**< cpu usage (sec + nanosec) */
    struct timespec cutime;     /**< child cpu usage (sec + nanosec) */
    struct timespec stime;      /**< sys cpu usage (sec + nanosec) */
    struct timespec cstime;     /**< child sys cpu usage (sec + nanosec) */
    unsigned long minflt;       /**< minor page faults (# of pages) */
    unsigned long cminflt;      /**< minor page faults of children */
    unsigned long majflt;       /**< major page faults (# of pages) */
    unsigned long cmajflt;      /**< major page faults of children */
    unsigned long vsize;        /**< virtual memory size (bytes) */
    long rss;                   /**< resident set size (pages) */
#ifdef __linux__
    unsigned long start_code;   /**< start address of the code segment */
    unsigned long end_code;     /**< end address of the code segment */
    unsigned long start_stack;  /**< start address of the stack */
    struct user_regs_struct regs; /**< registers */
    siginfo_t siginfo;          /**< signal information */
#endif /* __linux__ */
    struct 
    {
        unsigned char single_step:1;
        unsigned char is_in_syscall:1;
        unsigned char not_wait_execve:1;
        unsigned char reserved:5;
        unsigned char syscall_mode:8;
        pthread_t trace_id;
    } tflags;                   /**< trace flags, maintained by trace_*() */
    unsigned long op;           /**< current instruction */
} proc_t;

/* Macros for manipulating struct timespec */

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

/* Macros for composing conditional rvalue */
#define RVAL_IF(x)              ((x)?(
#define RVAL_ELSE               ):(
#define RVAL_FI                 ))

#define SET_IN_SYSCALL(pproc) \
{{{ \
    (pproc)->tflags.is_in_syscall = true; \
    (pproc)->tflags.not_wait_execve = true; \
}}} /* SET_IN_SYSCALL */

#define NOT_WAIT_EXECVE(pproc) \
    ((pproc)->tflags.not_wait_execve++) \
/* NOT_WAIT_EXECVE */

#ifdef __linux__

#ifdef __x86_64__

/* int80 and sysenter always maps in 32bit system call table regardless of the 
 * value of CS c.f. http://scary.beasts.org/security/CESA-2009-001.html */

#define SCMODE_LINUX64          0
#define SCMODE_LINUX32          1
#define SCMODE_MAX              2

#define SCMODE(pproc) \
    RVAL_IF(!((pproc)->tflags.is_in_syscall)) \
        ((pproc)->tflags.syscall_mode = \
            RVAL_IF((OPCODE((pproc)->op) == OP_INT80) || \
                    (OPCODE((pproc)->op) == OP_SYSENTER)) \
                SCMODE_LINUX32 \
            RVAL_ELSE \
                RVAL_IF(OPCODE((pproc)->op) == OP_SYSCALL) \
                    RVAL_IF((pproc)->regs.cs == 0x33) \
                        SCMODE_LINUX64 \
                    RVAL_ELSE \
                        RVAL_IF((pproc)->regs.cs == 0x23) \
                            SCMODE_LINUX32 \
                        RVAL_ELSE \
                            SCMODE_MAX \
                        RVAL_FI \
                    RVAL_FI \
                RVAL_ELSE \
                    SCMODE_MAX \
                RVAL_FI \
            RVAL_FI) \
    RVAL_ELSE \
        ((pproc)->tflags.syscall_mode) \
    RVAL_FI \
/* SCMODE */

#define THE_SYSCALL(pproc) \
    RVAL_IF(((pproc)->tflags.single_step) && !((pproc)->tflags.is_in_syscall)) \
        MAKE_WORD((pproc)->regs.rax, SCMODE(pproc)) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.orig_rax, SCMODE(pproc)) \
    RVAL_FI \
/* THE_SYSCALL */

#define SC_EXECVE               MAKE_WORD(SYS_execve, SCMODE_LINUX64)
#define SC_FORK                 MAKE_WORD(SYS_fork, SCMODE_LINUX64)
#define SC_VFORK                MAKE_WORD(SYS_vfork, SCMODE_LINUX64)
#define SC_CLONE                MAKE_WORD(SYS_clone, SCMODE_LINUX64)
#define SC_WAITPID              MAKE_WORD(SYS_waitpid, SCMODE_LINUX64)
#define SC_WAIT4                MAKE_WORD(SYS_wait4, SCMODE_LINUX64)
#define SC_WAITID               MAKE_WORD(SYS_waitid, SCMODE_LINUX64)
#define SC_EXIT                 MAKE_WORD(SYS_exit, SCMODE_LINUX64)
#define SC_EXIT_GROUP           MAKE_WORD(SYS_exit_group, SCMODE_LINUX64)

/* Hard coding these numbers is somewhat ugly, but we are in 64bit mode here,
 * and <sys/syscall.h> does not expose 32bit system call numbers. */

#define SC32_EXECVE             MAKE_WORD(11, SCMODE_LINUX32)
#define SC32_FORK               MAKE_WORD(2, SCMODE_LINUX32)
#define SC32_VFORK              MAKE_WORD(190, SCMODE_LINUX32)
#define SC32_CLONE              MAKE_WORD(120, SCMODE_LINUX32)
#define SC32_WAITPID            MAKE_WORD(7, SCMODE_LINUX32)
#define SC32_WAIT4              MAKE_WORD(114, SCMODE_LINUX32)
#define SC32_WAITID             MAKE_WORD(284, SCMODE_LINUX32)
#define SC32_EXIT               MAKE_WORD(1, SCMODE_LINUX32)
#define SC32_EXIT_GROUP         MAKE_WORD(252, SCMODE_LINUX32)

#define IS_SYSCALL(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE((pproc)->op) == OP_SYSCALL) || \
         (OPCODE((pproc)->op) == OP_SYSENTER) || \
         (OPCODE((pproc)->op) == OP_INT80)) \
    RVAL_ELSE \
        (((pproc)->siginfo.si_signo == SIGTRAP) && \
         ((pproc)->siginfo.si_code != SI_USER)) \
    RVAL_FI \
/* IS_SYSCALL */

#define IS_SYSRET(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE((pproc)->op) != OP_SYSCALL) && \
         (OPCODE((pproc)->op) != OP_SYSENTER) && \
         (OPCODE((pproc)->op) != OP_INT80) && \
         ((pproc)->tflags.is_in_syscall)) \
    RVAL_ELSE \
        (((pproc)->siginfo.si_signo == SIGTRAP) && \
         ((pproc)->siginfo.si_code != SI_USER)) \
    RVAL_FI \
/* IS_SYSRET */

#define SYSCALL_ARG1(pproc) \
    RVAL_IF(SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rdi) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rbx, 0) \
    RVAL_FI \
/* SYSCALL_ARG1 */
#define SYSCALL_ARG2(pproc) \
    RVAL_IF(SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rsi) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rcx, 0) \
    RVAL_FI \
/* SYSCALL_ARG2 */
#define SYSCALL_ARG3(pproc) \
    RVAL_IF(SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rdx) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rdx, 0) \
    RVAL_FI \
/* SYSCALL_ARG3 */
#define SYSCALL_ARG4(pproc) \
    RVAL_IF(SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rcx) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rsi, 0) \
    RVAL_FI \
/* SYSCALL_ARG4 */
#define SYSCALL_ARG5(pproc) \
    RVAL_IF(SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.r8) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rdi, 0) \
    RVAL_FI \
/* SYSCALL_ARG5 */
#define SYSCALL_ARG6(pproc) \
    RVAL_IF(SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.r9) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rbp, 0) \
    RVAL_FI \
/* SYSCALL_ARG6 */
#define SYSRET_RETVAL(pproc) \
    RVAL_IF(SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rax) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rax, 0) \
    RVAL_FI \
/* SYSCALL_RETVAL */

#define CLR_IN_SYSCALL(pproc) \
{{{ \
    (pproc)->tflags.is_in_syscall = false; \
    (pproc)->tflags.not_wait_execve = ((THE_SYSCALL(pproc) != SC_EXECVE) && \
                                       (THE_SYSCALL(pproc) != SC32_EXECVE)); \
}}} /* CLR_IN_SYSCALL */

#else /* __i386__ */

#define SCMODE_LINUX32          0
#define SCMODE_MAX              1

#define SCMODE(pproc) \
    RVAL_IF(((pproc)->regs.xcs == 0x73) || ((pproc)->regs.xcs == 0x23)) \
        SCMODE_LINUX32 \
    RVAL_ELSE \
        SCMODE_MAX \
    RVAL_FI \
/* SCMODE */

#define THE_SYSCALL(pproc) \
    RVAL_IF(((pproc)->tflags.single_step) && !((pproc)->tflags.is_in_syscall)) \
        MAKE_WORD((pproc)->regs.eax, SCMODE_LINUX32) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.orig_eax, SCMODE_LINUX32) \
    RVAL_FI \
/* THE_SYSCALL */

#define SC_EXECVE               MAKE_WORD(SYS_execve, SCMODE_LINUX32)
#define SC_FORK                 MAKE_WORD(SYS_fork, SCMODE_LINUX32)
#define SC_VFORK                MAKE_WORD(SYS_vfork, SCMODE_LINUX32)
#define SC_CLONE                MAKE_WORD(SYS_clone, SCMODE_LINUX32)
#define SC_WAITPID              MAKE_WORD(SYS_waitpid, SCMODE_LINUX32)
#define SC_WAIT4                MAKE_WORD(SYS_wait4, SCMODE_LINUX32)
#define SC_WAITID               MAKE_WORD(SYS_waitid, SCMODE_LINUX32)
#define SC_EXIT                 MAKE_WORD(SYS_exit, SCMODE_LINUX32)
#define SC_EXIT_GROUP           MAKE_WORD(SYS_exit_group, SCMODE_LINUX32)

#define IS_SYSCALL(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE((pproc)->op) == OP_INT80) || \
         (OPCODE((pproc)->op) == OP_SYSENTER)) \
    RVAL_ELSE \
        (((pproc)->siginfo.si_signo == SIGTRAP) && \
         ((pproc)->siginfo.si_code != SI_USER)) \
    RVAL_FI \
/* IS_SYSCALL */

#define IS_SYSRET(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE((pproc)->op) != OP_INT80) && \
         (OPCODE((pproc)->op) != OP_SYSENTER) && \
         ((pproc)->tflags.is_in_syscall)) \
    RVAL_ELSE \
        (((pproc)->siginfo.si_signo == SIGTRAP) && \
         ((pproc)->siginfo.si_code != SI_USER)) \
    RVAL_FI \
/* IS_SYSRET */

#define SYSCALL_ARG1(pproc)     ((pproc)->regs.ebx)
#define SYSCALL_ARG2(pproc)     ((pproc)->regs.ecx)
#define SYSCALL_ARG3(pproc)     ((pproc)->regs.edx)
#define SYSCALL_ARG4(pproc)     ((pproc)->regs.esi)
#define SYSCALL_ARG5(pproc)     ((pproc)->regs.edi)
#define SYSCALL_ARG6(pproc)     ((pproc)->regs.ebp)
#define SYSRET_RETVAL(pproc)    ((pproc)->regs.eax)

#define CLR_IN_SYSCALL(pproc) \
{{{ \
    (pproc)->tflags.is_in_syscall = false; \
    (pproc)->tflags.not_wait_execve = (THE_SYSCALL(pproc) != SC_EXECVE); \
}}} /* CLR_IN_SYSCALL */

#endif /* __x86_64__ */

#else /* __linux__ */

#define SCMODE(pproc) 0
#error "SCMODE is not implemented for this platform"
#define THE_SYSCALL(pproc) 0
#error "THE_SYSCALL is not implemented for this platform"
#define IS_SYSCALL(pproc) (true)
#error "IS_SYSCALL is not implemented for this platform"
#define IS_SYSRET(pproc) (true)
#error "IS_SYSRET is not implemented for this platform"
#define SYSCALL_ARG1(pproc) 0
#define SYSCALL_ARG2(pproc) 0
#define SYSCALL_ARG3(pproc) 0
#define SYSCALL_ARG4(pproc) 0
#define SYSCALL_ARG5(pproc) 0
#define SYSCALL_ARG6(pproc) 0
#define SYSRET_RETVAL(pproc) 0

#endif /* __linux__ */

/**
 * @brief Bind an empty process stat buffer with a sandbox instance.
 * @param[in] psbox pointer to the sandbox instance
 * @param[out] pproc target process stat buffer
 * @return true on success
 */
bool proc_bind(const void * const psbox, proc_t * const pproc);

/**
 * @brief Process probing options.
 */
typedef enum
{
    PROBE_STAT = 0,             /**< probe procfs for process status */
    PROBE_REGS = 1,             /**< probe user registers */
    PROBE_OP = 2,               /**< probe current instruction */
    PROBE_SIGINFO = 4,          /**< probe signal info */
} probe_option_t;

/**
 * @brief Probe runtime information of specified process.
 * @param[in] pid id of targeted process
 * @param[in] opt probe options (can be bitwise OR of PROB_STAT, PROB_REGS,
 *            PROBE_OP, and PROBE_SIGINFO)
 * @param[out] pproc process stat buffer
 * @return true on sucess, false otherwise
 */
bool proc_probe(pid_t pid, int opt, proc_t * const pproc);

/**
 * @brief Copy a word from the specified address of targeted process.
 * @param[in] pproc process stat buffer with valid pid and state
 * @param[out] addr targeted address of the given process
 * @param[out] pword pointer to a buffer at least 4 bytes in length
 * @return true on success, false otherwise
 */
bool proc_dump(const proc_t * const pproc, const void * const addr, 
               long * const pword);

/**
 * @brief Subprocess trace methods.
 */
typedef enum 
{
    TRACE_SINGLE_STEP = 0,      /**< enter single-step tracing mode */
    TRACE_SYSTEM_CALL = 1,      /**< enter system call tracing mode */
} trace_type_t;

/**
 * @brief Let the current process enter traced state.
 * @return true on success
 */
bool trace_self(void);

/**
 * @brief Hack the traced process to ensure safe tracing.
 * @return true on success
 */
bool trace_hack(proc_t * const pproc);

/**
 * @brief Schedule next stop for a traced process.
 * @param[in,out] pproc target process stat buffer with valid pid
 * @param[in] type \c TRACE_SYSTEM_CALL or \c TRACE_SINGLE_STEP
 * @return true on success
 */
bool trace_next(proc_t * const pproc, trace_type_t type);

/**
 * @brief Kill a traced process, prevent any overrun.
 * @param[in,out] pproc target process stat buffer with valid pid
 * @param[in] signal type of signal to use
 * @return true on success
 */
bool trace_kill(proc_t * const pproc, int signal);

/**
 * @brief Main thread that actually performs trace_*() functions.
 * @param[in,out] psbox pointer to an initialized sandbox
 * @return the same pointer as input
 */
void * trace_main(void * const psbox);

/**
 * @brief Terminate a traced process and quit trace_main().
 * @param[in] pproc target process stat buffer with valid pid and flags
 * @return true on success
 */
bool trace_end(const proc_t * const pproc);

/**
 * @brief Evict the existing blocks from the data caches.
 * This function was taken from Chapter 9 of Computer Systems A Programmer's
 * Perspective by Randal E. Bryant and David R. O'Hallaron
 */
void cache_flush(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_PLATFORM_H__ */
