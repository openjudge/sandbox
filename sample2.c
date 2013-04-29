/*******************************************************************************
 * The Sandbox Libraries (Core) - C Sample Program                             *
 *                                                                             *
 * Copyright (C) 2012-2013 LIU Yu, pineapple.liu@gmail.com                     *
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

/* check platform type */
#if !defined(__linux__) || (!defined(__x86_64__) && !defined(__i386__))
#error "Unsupported platform type"
#endif /**/

#ifndef PROG_NAME
#define PROG_NAME "sample2"
#endif /* PROG_NAME */

#include <assert.h>
#include <sandbox.h>
#include <stdint.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#ifndef INT16_MAX
#define INT16_MAX (32767)
#endif /* INT16_MAX */

/* mini sandbox with embedded policy */
typedef action_t* (*rule_t)(const sandbox_t*, const event_t*, action_t*);
typedef struct
{
   sandbox_t sbox;
   policy_t default_policy;
   rule_t sc_table[INT16_MAX + 1];
} minisbox_t;

/* result code translation table */
const char* result_name[] =
{
    "PD", "OK", "RF", "ML", "OL", "TL", "RT", "AT", "IE", "BP", NULL,
};

/* initialize and apply local policy rules */
void policy_setup(minisbox_t*);

typedef enum
{
    P_ELAPSED = 0, P_CPU = 1, P_MEMORY = 2,
} probe_t;

res_t probe(const sandbox_t*, probe_t);

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "synopsis: " PROG_NAME " foo/bar.exe\n");
        return EX_USAGE;
    }
    /* create and configure a sandbox instance */
    minisbox_t msb;
    if (sandbox_init(&msb.sbox, &argv[1]) != 0)
    {
        fprintf(stderr, "sandbox initialization failed\n");
        return EX_DATAERR;
    }
    policy_setup(&msb);
    msb.sbox.task.ifd = STDIN_FILENO;  /* input to targeted program */
    msb.sbox.task.ofd = STDOUT_FILENO; /* output from targeted program */
    msb.sbox.task.efd = STDERR_FILENO; /* error from targeted program */
    msb.sbox.task.quota[S_QUOTA_WALLCLOCK] = 30000; /* 30 sec */
    msb.sbox.task.quota[S_QUOTA_CPU] = 2000;        /*  2 sec */
    msb.sbox.task.quota[S_QUOTA_MEMORY] = 8388608;  /*  8 MB  */
    msb.sbox.task.quota[S_QUOTA_DISK] = 1048576;    /*  1 MB  */
    /* execute till end */
    if (!sandbox_check(&msb.sbox))
    {
        fprintf(stderr, "sandbox pre-execution state check failed\n");
        return EX_DATAERR;
    }
    result_t res = *sandbox_execute(&msb.sbox);
    /* verbose statistics */
    fprintf(stderr, "result: %s\n", result_name[res]);
    fprintf(stderr, "cpu: %ldms\n", probe(&msb.sbox, P_CPU));
    fprintf(stderr, "mem: %ldkB\n", probe(&msb.sbox, P_MEMORY));
    /* destroy sandbox instance */
    sandbox_fini(&msb.sbox);
    return EX_OK;
}

/* convert struct timespec to msec equivalent */
unsigned long ts2ms(struct timespec ts)
{
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

res_t probe(const sandbox_t* psbox, probe_t key)
{
    switch (key)
    {
    case P_ELAPSED:
        return ts2ms(psbox->stat.elapsed);
    case P_CPU:
        return ts2ms(psbox->stat.cpu_info.clock);
    case P_MEMORY:
        return psbox->stat.mem_info.vsize_peak / 1024;
    default:
        break;
    }
    return 0;
}

/* convert syscall_t to 15bit sc_table index */
int16_t sc2idx(syscall_t scinfo)
{
#ifdef __x86_64__
    assert((scinfo.scno >= 0) && (scinfo.scno < 1024) && 
        (scinfo.mode >= 0) && (scinfo.mode < 32));
    return (int16_t)(scinfo.scno | (scinfo.mode << 10));
#else /* __i386__ */
    assert((scinfo >= 0) && (scinfo < 1024));
    return (int16_t)(scinfo);
#endif /* __x86_64__ */
}

/* tag syscall number with linux32 abi mask */
int16_t abi32(int scno)
{
    assert((scno >= 0) && (scno < 1024));
#ifdef __x86_64__
    return (int16_t)(scno | (1 << 10));
#else /* __i386__ */
    return (int16_t)(scno);
#endif /* __x86_64__ */
}

void policy(const policy_t*, const event_t*, action_t*);
action_t* _KILL_RF(const sandbox_t*, const event_t*, action_t*);
action_t* _CONT(const sandbox_t*, const event_t*, action_t*);

void policy_setup(minisbox_t* pmsb)
{
    assert(pmsb);
    /* white list of essential linux syscalls for statically-linked C programs */
    const int16_t sc_safe[] =
    {
        abi32(0), abi32(3), abi32(4), abi32(19), abi32(45), abi32(54), 
        abi32(90), abi32(91), abi32(122), abi32(125), abi32(140), abi32(163),
        abi32(192), abi32(197), abi32(224), abi32(243), abi32(252), 
#ifdef __x86_64__
        0, 1, 5, 8, 9, 10, 11, 12, 16, 25, 63, 158, 219, 231, 
#endif /* __x86_64__ */
        -1, /* sentinel */
    };
    /* initialize table of system call rules */
    int sc, i = 0;
    for (sc = 0; sc <= INT16_MAX; sc++)
    {
        pmsb->sc_table[sc] = _KILL_RF;
    }
    while ((sc = sc_safe[i++]) >= 0)
    {
        pmsb->sc_table[sc] = _CONT;
    }
    /* override the default policy of the sandbox */
    pmsb->default_policy = pmsb->sbox.ctrl.policy;
    pmsb->default_policy.entry = (pmsb->default_policy.entry) ?: \
        (void*)sandbox_default_policy;
    pmsb->sbox.ctrl.policy = (policy_t){(void*)policy, (long)pmsb};
}

void policy(const policy_t* pp, const event_t* pe, action_t* pa)
{
    assert(pp && pe && pa);
    const minisbox_t* pmsb = (const minisbox_t*)pp->data;
    /* handle SYSCALL/SYSRET events with local rules */
    if ((pe->type == S_EVENT_SYSCALL) || (pe->type == S_EVENT_SYSRET))
    {
        const syscall_t scinfo = *(const syscall_t*)&(pe->data._SYSCALL.scinfo);
        pmsb->sc_table[sc2idx(scinfo)](&pmsb->sbox, pe, pa);
        return;
    }
    /* bypass other events to the default poicy */
    ((policy_entry_t)pmsb->default_policy.entry)(&pmsb->default_policy, pe, pa);
}

action_t* _CONT(const sandbox_t* psbox, const event_t* pe, action_t* pa)
{
    *pa = (action_t){S_ACTION_CONT}; /* continue */
    return pa;
}

action_t* _KILL_RF(const sandbox_t* psbox, const event_t* pe, action_t* pa)
{
    *pa = (action_t){S_ACTION_KILL, {{S_RESULT_RF}}}; /* restricted func. */
    return pa;
}

