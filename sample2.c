/*******************************************************************************
 * The Sandbox Libraries (Core) - C Sample Program                             *
 *                                                                             *
 * Copyright (C) 2012 LIU Yu, pineapple.liu@gmail.com                          *
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

#include <stdio.h>
#include <assert.h>
#include <sysexits.h>
#include <unistd.h>
#include <sandbox.h>

/* mini sandbox with embedded policy */
typedef action_t* (*rule_t)(const sandbox_t*, const event_t*, action_t*);
typedef struct
{
   sandbox_t sbox;
   policy_t default_policy;
   rule_t sc_table[1024];
} minisbox_t;

void policy_setup(minisbox_t*);

typedef enum {P_ELAPSED = 0, P_CPU = 1, P_MEMORY = 2, } probe_t;
res_t probe(const sandbox_t*, probe_t);

/* result code translation table */
const char* result_name[] =
{
    "PD", "OK", "RF", "ML", "OL", "TL", "RT", "AT", "IE", "BP", NULL,
};

int 
main(int argc, const char* argv[])
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

/* struct timespec to msec conversion */
static unsigned long
ts2ms(struct timespec ts)
{
    return ts.tv_sec * 1000 + ts.tv_nsec / 100000;
}

res_t 
probe(const sandbox_t* psbox, probe_t key)
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

static void policy(const policy_t*, const event_t*, action_t*);
static action_t* _KILL_RF(const sandbox_t*, const event_t*, action_t*);
static action_t* _CONT(const sandbox_t*, const event_t*, action_t*);

/* white list of essential linux syscalls */
const int sc_safe[] =
{
#ifdef __x86_64__
    0, 1, 5, 8, 9, 10, 11, 12, 16, 25, 63, 158, 231, -1,
#else /* __i386__ */
    3, 4, 19, 45, 54, 90, 91, 122, 125, 140, 163, 192, 197, 224, 243, 252, -1,
#endif /* __x86_64__ */
};

void
policy_setup(minisbox_t* pmsb)
{
    assert(pmsb);
    /* initialize table of system call rules */
    size_t scno, i = 0;
    for (scno = 0; scno < sizeof(pmsb->sc_table) / sizeof(rule_t); scno++)
    {
        pmsb->sc_table[scno] = _KILL_RF;
    }
    while (sc_safe[i] >= 0)
    {
        pmsb->sc_table[sc_safe[i++]] = _CONT;
    }
    pmsb->default_policy = pmsb->sbox.ctrl.policy;
    pmsb->default_policy.entry = (pmsb->default_policy.entry) ?: \
        (void*)sandbox_default_policy;
    pmsb->sbox.ctrl.policy = (policy_t){(void*)policy, (long)pmsb};
}

static void
policy(const policy_t* pp, const event_t* pe, action_t* pa)
{
    assert(pp && pe && pa);
    const minisbox_t* pmsb = (const minisbox_t*)pp->data;
    /* handle SYSCALL/SYSRET events with local rules */
    if ((pe->type == S_EVENT_SYSCALL) || (pe->type == S_EVENT_SYSRET))
    {
        const syscall_t scinfo = *(const syscall_t*)&(pe->data._SYSCALL.scinfo);
#ifdef __x86_64__
        if (scinfo.mode != 0)
        {
            _KILL_RF(&pmsb->sbox, pe, pa);
            return;
        }
        pmsb->sc_table[scinfo.scno](&pmsb->sbox, pe, pa);
#else /* __i386__ */
        pmsb->sc_table[scinfo](&pmsb->sbox, pe, pa);
#endif /* __x86_64__ */
        return;
    }
    /* bypass other events to the default poicy */
    ((policy_entry_t)pmsb->default_policy.entry)(&pmsb->default_policy, pe, pa);
}

static action_t*
_CONT(const sandbox_t* psbox, const event_t* pe, action_t* pa)
{
    *pa = (action_t){S_ACTION_CONT};
    return pa;
}

static action_t*
_KILL_RF(const sandbox_t* psbox, const event_t* pe, action_t* pa)
{
    *pa = (action_t){S_ACTION_KILL, {{S_RESULT_RF}}};
    return pa;
}

