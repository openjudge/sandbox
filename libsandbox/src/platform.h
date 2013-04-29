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
 * @file platform.h 
 * @brief Platform dependent features wrapper.
 */
#ifndef __OJS_PLATFORM_H__
#define __OJS_PLATFORM_H__

#include <pthread.h>            /* pthread_t */
#include <stdbool.h>            /* false, true */
#include <signal.h>             /* siginfo_t, SIGTRAP, ... */
#include <sys/types.h>          /* pid_t */
#ifdef __linux__
#include <sys/reg.h>            /* EAX, EBX, ... */
#include <sys/syscall.h>        /* SYS_execve, ... */
#include <sys/user.h>           /* struct user_regs_struct */
#endif /* __linux__ */
#include <time.h>               /* struct timespec */

#ifdef __cplusplus
extern "C"
{
#endif

/* architecture detection */
#undef SUPPORTED_ARCH

#ifdef __x86_64__
#define SUPPORTED_ARCH
#ifndef __WORDSIZE
#define __WORDSIZE              64
#endif /* __WORDSIZE */
#define MAKE_WORD(a,b) \
    ((unsigned long)((((unsigned long)((unsigned int)(b))) << 32) | \
                     ((unsigned int)(a)))) \
/* MAKE_WORD */
#define OPCODE16(op) \
    ((unsigned long)(op) & (~((unsigned long)(~0LU << 16)))) \
/* OPCODE16 */
#define OP_INT80                0x80cd
#define OP_SYSCALL              0x050f
#define OP_SYSENTER             0x340f
#define OP_NOP                  0x90
#define NIP                     rip
#define NAX                     rax
#define ORIG_NAX                orig_rax
#endif /* __x86_64__ */

#ifdef __i386__
#define SUPPORTED_ARCH
#ifndef __WORDSIZE
#define __WORDSIZE              32
#endif /* __WORDSIZE */
#define MAKE_WORD(a,b) \
    ((unsigned long)((((unsigned long)((unsigned short)(b))) << 16) | \
                     ((unsigned short)(a)))) \
/* MAKE_WORD */
#define OPCODE16(op) \
    ((unsigned long)(op) & (~((unsigned long)(~0LU << 16)))) \
/* OPCODE16 */
#define OP_INT80                0x80cd
#define OP_SYSENTER             0x340f
#define OP_NOP                  0x90
#define NIP                     eip
#define NAX                     eax
#define ORIG_NAX                orig_eax
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
    struct timespec stime;      /**< sys cpu usage (sec + nanosec) */
    unsigned long minflt;       /**< minor page faults (# of pages) */
    unsigned long majflt;       /**< major page faults (# of pages) */
    unsigned long vsize;        /**< virtual memory size (bytes) */
    long rss;                   /**< resident set size (pages) */
#ifdef __linux__
    unsigned long start_code;   /**< start address of the code segment */
    unsigned long end_code;     /**< end address of the code segment */
    unsigned long start_stack;  /**< start address of the stack */
    struct user_regs_struct regs; /**< registers */
#endif /* __linux__ */
    siginfo_t siginfo;          /**< signal information */
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

#define NOT_WAIT_EXECVE(pproc) \
    ((pproc)->tflags.not_wait_execve++) \
/* NOT_WAIT_EXECVE */

/**
 * @brief Bind an empty process stat buffer with a sandbox instance.
 * @param[in] psbox pointer to the sandbox instance
 * @param[out] pproc pointer to any process stat buffer
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
    PROBE_OP = 3,               /**< probe current instruction */
    PROBE_SIGINFO = 4,          /**< probe signal info */
} probe_option_t;

/**
 * @brief Probe runtime information of specified process.
 * @param[in] pid id of the prisoner process
 * @param[in] opt probe options (can be bitwise OR of PROB_STAT, PROB_REGS,
 *            PROBE_OP, and PROBE_SIGINFO)
 * @param[out] pproc pointer to a binded process stat buffer
 * @return true on sucess, false otherwise
 */
bool proc_probe(pid_t pid, int opt, proc_t * const pproc);

/**
 * @brief Dump data from the memory space of the prisoner process
 * @param[in] pproc pointer to a binded process stat buffer
 * @param[in] addr target address in the memory space of the prisoner process
 * @param[in] len number of bytes to dump
 * @param[in,out] buff data buffer in local space
 * @return true on success, false otherwise
 */
bool proc_dump(const proc_t * const pproc, const void * const addr, 
               size_t len, char * const buff);

/**
 * @brief Inspect current system call type of a process.
 * @param[in,out] pproc pointer to an initialized procss stat buffer with
 * \c regs and \op fields correctly filled by \c proc_probe()
 * @return 0 for native type, 1, 2, ... for valid alternative types, and 
 * \c SCMODE_MAX for unknown type
 */
int proc_abi(proc_t * const);

#ifdef __linux__

#define THE_SCMODE(pproc) \
    RVAL_IF(!((pproc)->tflags.is_in_syscall) && \
            ((pproc)->tflags.not_wait_execve)) \
        (pproc)->tflags.syscall_mode = proc_abi(pproc) \
    RVAL_ELSE \
        (pproc)->tflags.syscall_mode \
    RVAL_FI \
/* THE_SCMODE */

#define THE_SYSCALL(pproc) \
    RVAL_IF(((pproc)->tflags.single_step) && !((pproc)->tflags.is_in_syscall)) \
        MAKE_WORD((pproc)->regs.NAX, THE_SCMODE(pproc)) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.ORIG_NAX, THE_SCMODE(pproc)) \
    RVAL_FI \
/* THE_SYSCALL */

#ifdef __x86_64__

#define SCMODE_LINUX64          0
#define SCMODE_LINUX32          1
#define SCMODE_MAX              2

#define SC_EXECVE               MAKE_WORD(SYS_execve, SCMODE_LINUX64)
#define SC_FORK                 MAKE_WORD(SYS_fork, SCMODE_LINUX64)
#define SC_VFORK                MAKE_WORD(SYS_vfork, SCMODE_LINUX64)
#define SC_CLONE                MAKE_WORD(SYS_clone, SCMODE_LINUX64)
#define SC_PTRACE               MAKE_WORD(SYS_ptrace, SCMODE_LINUX64)
#ifdef SYS_waitpid
#define SC_WAITPID              MAKE_WORD(SYS_waitpid, SCMODE_LINUX64)
#endif /* SYS_waitpid */
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
#define SC32_PTRACE             MAKE_WORD(26, SCMODE_LINUX32)
#define SC32_WAITPID            MAKE_WORD(7, SCMODE_LINUX32)
#define SC32_WAIT4              MAKE_WORD(114, SCMODE_LINUX32)
#define SC32_WAITID             MAKE_WORD(284, SCMODE_LINUX32)
#define SC32_EXIT               MAKE_WORD(1, SCMODE_LINUX32)
#define SC32_EXIT_GROUP         MAKE_WORD(252, SCMODE_LINUX32)

#define IS_SYSCALL(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE16((pproc)->op) == OP_SYSCALL) || \
         (OPCODE16((pproc)->op) == OP_SYSENTER) || \
         (OPCODE16((pproc)->op) == OP_INT80)) \
    RVAL_ELSE \
        (((pproc)->siginfo.si_signo == SIGTRAP) && \
         ((pproc)->siginfo.si_code != SI_USER)) \
    RVAL_FI \
/* IS_SYSCALL */

#define IS_SYSRET(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE16((pproc)->op) != OP_SYSCALL) && \
         (OPCODE16((pproc)->op) != OP_SYSENTER) && \
         (OPCODE16((pproc)->op) != OP_INT80) && \
         ((pproc)->tflags.is_in_syscall)) \
    RVAL_ELSE \
        (((pproc)->siginfo.si_signo == SIGTRAP) && \
         ((pproc)->siginfo.si_code != SI_USER)) \
    RVAL_FI \
/* IS_SYSRET */

#define SYSCALL_ARG1(pproc) \
    RVAL_IF(THE_SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rdi) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rbx, 0) \
    RVAL_FI \
/* SYSCALL_ARG1 */

#define SYSCALL_ARG2(pproc) \
    RVAL_IF(THE_SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rsi) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rcx, 0) \
    RVAL_FI \
/* SYSCALL_ARG2 */

#define SYSCALL_ARG3(pproc) \
    RVAL_IF(THE_SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rdx) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rdx, 0) \
    RVAL_FI \
/* SYSCALL_ARG3 */

#define SYSCALL_ARG4(pproc) \
    RVAL_IF(THE_SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.r10) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rsi, 0) \
    RVAL_FI \
/* SYSCALL_ARG4 */

#define SYSCALL_ARG5(pproc) \
    RVAL_IF(THE_SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.r8) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rdi, 0) \
    RVAL_FI \
/* SYSCALL_ARG5 */

#define SYSCALL_ARG6(pproc) \
    RVAL_IF(THE_SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.r9) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rbp, 0) \
    RVAL_FI \
/* SYSCALL_ARG6 */

#define SYSRET_RETVAL(pproc) \
    RVAL_IF(THE_SCMODE(pproc) == SCMODE_LINUX64) \
        ((pproc)->regs.rax) \
    RVAL_ELSE \
        MAKE_WORD((pproc)->regs.rax, 0) \
    RVAL_FI \
/* SYSCALL_RETVAL */

#define SET_IN_SYSCALL(pproc) \
{{{ \
    (pproc)->tflags.is_in_syscall = true; \
    (pproc)->tflags.not_wait_execve = ((THE_SYSCALL(pproc) != SC_EXECVE) && \
                                       (THE_SYSCALL(pproc) != SC32_EXECVE)); \
}}} /* SET_IN_SYSCALL */

#define CLR_IN_SYSCALL(pproc) \
{{{ \
    (pproc)->tflags.is_in_syscall = false; \
    (pproc)->tflags.not_wait_execve = ((THE_SYSCALL(pproc) != SC_EXECVE) && \
                                       (THE_SYSCALL(pproc) != SC32_EXECVE)); \
}}} /* CLR_IN_SYSCALL */

#else /* __i386__ */

#define SCMODE_LINUX32          0
#define SCMODE_MAX              1

#define SC_EXECVE               MAKE_WORD(SYS_execve, SCMODE_LINUX32)
#define SC_FORK                 MAKE_WORD(SYS_fork, SCMODE_LINUX32)
#define SC_VFORK                MAKE_WORD(SYS_vfork, SCMODE_LINUX32)
#define SC_CLONE                MAKE_WORD(SYS_clone, SCMODE_LINUX32)
#define SC_PTRACE               MAKE_WORD(SYS_ptrace, SCMODE_LINUX32)
#define SC_WAITPID              MAKE_WORD(SYS_waitpid, SCMODE_LINUX32)
#define SC_WAIT4                MAKE_WORD(SYS_wait4, SCMODE_LINUX32)
#define SC_WAITID               MAKE_WORD(SYS_waitid, SCMODE_LINUX32)
#define SC_EXIT                 MAKE_WORD(SYS_exit, SCMODE_LINUX32)
#define SC_EXIT_GROUP           MAKE_WORD(SYS_exit_group, SCMODE_LINUX32)

#define IS_SYSCALL(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE16((pproc)->op) == OP_INT80) || \
         (OPCODE16((pproc)->op) == OP_SYSENTER)) \
    RVAL_ELSE \
        (((pproc)->siginfo.si_signo == SIGTRAP) && \
         ((pproc)->siginfo.si_code != SI_USER)) \
    RVAL_FI \
/* IS_SYSCALL */

#define IS_SYSRET(pproc) \
    RVAL_IF((pproc)->tflags.single_step) \
        ((OPCODE16((pproc)->op) != OP_INT80) && \
         (OPCODE16((pproc)->op) != OP_SYSENTER) && \
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

#define SET_IN_SYSCALL(pproc) \
{{{ \
    (pproc)->tflags.is_in_syscall = true; \
    (pproc)->tflags.not_wait_execve = (THE_SYSCALL(pproc) != SC_EXECVE); \
}}} /* SET_IN_SYSCALL */

#define CLR_IN_SYSCALL(pproc) \
{{{ \
    (pproc)->tflags.is_in_syscall = false; \
    (pproc)->tflags.not_wait_execve = (THE_SYSCALL(pproc) != SC_EXECVE); \
}}} /* CLR_IN_SYSCALL */

#endif /* __x86_64__ */

#else /* !defined(__linux__) */

#define THE_SCMODE(pproc) 0
#error "THE_SCMODE is not implemented for this platform"
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
 * @brief Let the current process enter traced state.
 * @return true on success
 */
bool trace_me(void);

/**
 * @brief Subprocess trace methods.
 */
typedef enum 
{
    TRACE_SINGLE_STEP = 0,      /**< enter single-step tracing mode */
    TRACE_SYSTEM_CALL = 1,      /**< enter system call tracing mode */
} trace_type_t;

/**
 * @brief Schedule next stop for a traced process.
 * @param[in,out] pproc pointer to a binded process stat buffer
 * @param[in] type \c TRACE_SYSTEM_CALL or \c TRACE_SINGLE_STEP
 * @return true on success
 */
bool trace_next(proc_t * const pproc, trace_type_t type);

/**
 * @brief Kill a traced process, prevent any overrun.
 * @param[in] pproc pointer to a binded process stat buffer
 * @param[in] signal type of signal to use
 * @return true on success
 */
bool trace_kill(const proc_t * const pproc, int signal);

/**
 * @brief Terminate a traced process and quit sandbox_tracer().
 * @param[in] pproc pointer to a binded process stat buffer
 * @return true on success
 */
bool trace_end(const proc_t * const pproc);

/**
 * @brief Service thread that actually performs trace_*() operations.
 * @param[in,out] psbox pointer to an initialized sandbox
 * @return the same pointer as input
 */
void * sandbox_tracer(void * const psbox);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_PLATFORM_H__ */
