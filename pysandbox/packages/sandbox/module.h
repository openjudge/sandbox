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
#ifndef __OJS_MODULE_H__
#define __OJS_MODULE_H__

/* <sandbox.h> SHOULD be included before <Python.h>, because the latter alters
 * macro definitions in standard C headers in a werid way, and may cause structs
 * in <sandbox.h> to become inconsistent with the pre-compiled libsandbox. Such 
 * inconsistency sometimes crashes the sandbox during initialization. */

#include <sandbox.h>

#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include <Python.h>
#include <structmember.h>

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K 1
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct 
{
    PyObject_HEAD
    event_t raw;
} SandboxEvent;

typedef struct 
{
    PyObject_HEAD
    action_t raw;
} SandboxAction;

typedef struct 
{
    PyObject_HEAD
} SandboxPolicy;

typedef struct 
{
    PyObject_HEAD
    sandbox_t sbox;
} Sandbox;

/* Inline help messages */

#ifndef DOC_TP_EVENT
#define DOC_TP_EVENT            0
#endif /* DOC_TP_EVENT */

#ifndef DOC_TP_ACTION
#define DOC_TP_ACTION           0
#endif /* DOC_TP_ACTION */

#ifndef DOC_TP_POLICY
#define DOC_TP_POLICY           0
#endif /* DOC_TP_POLICY */

#ifndef DOC_TP_SANDBOX
#define DOC_TP_SANDBOX          0
#endif /* DOC_TP_SANDBOX */

#ifndef DOC_SANDBOX_STATUS
#define DOC_SANDBOX_STATUS      "Runtime state of the sandbox instance "\
                                "(can be any of S_STATUS_*)"
#endif /* DOC_SANDBOX_STATUS */

#ifndef DOC_SANDBOX_RESULT
#define DOC_SANDBOX_RESULT      "Terminating state of the targeted program " \
                                "(can be any of S_RESULT_*)"
#endif /* DOC_SANDBOX_RESULT */

#ifndef DOC_SANDBOX_DUMP
#define DOC_SANDBOX_DUMP        "Dump an object from the memory space of the " \
                                "targeted program"
#endif /* DOC_SANDBOX_DUMP */

#ifndef DOC_SANDBOX_PROBE    
#define DOC_SANDBOX_PROBE       "Probe the statistics collected by the sandbox"
#endif /* DOC_SANDBOX_PROBE */

#ifndef DOC_SANDBOX_RUN
#define DOC_SANDBOX_RUN         "Start to run the task and wait for its " \
                                "completion"
#endif /* DOC_SANDBOX_RUN */

/* Error messages */

#ifdef IS_PY3K
#define MSG_STR_OBJ             "bytes or string object"
#else
#define MSG_STR_OBJ             "string or unicode object"
#endif
#define MSG_STR_OR_SEQ          "string or sequence of strings"
#define MSG_STR_TYPE_ERR        "expect a " MSG_STR_OBJ

#define MSG_ARGS_TOO_LONG       "command line is too long"
#define MSG_ARGS_TYPE_ERR       "command line should be a " MSG_STR_OR_SEQ
#define MSG_ARGS_VAL_ERR        "command line should be a " MSG_STR_OR_SEQ
#define MSG_ARGS_INVALID        "command line should contain full path to an " \
                                "executable"

#define MSG_JAIL_TOO_LONG       "program jail is too long"
#define MSG_JAIL_TYPE_ERR       "program jail should be a " MSG_STR_OBJ
#define MSG_JAIL_INVALID        "program jail should be a valid path"
#define MSG_JAIL_NOPERM         "only super-user can chroot"

#define MSG_OWNER_MISSING       "no such user"
#define MSG_OWNER_TYPE_ERR      "owner should be a valid user name or uid"
#define MSG_OWNER_NOPERM        "only super-user can setuid"

#define MSG_GROUP_MISSING       "no such group"
#define MSG_GROUP_TYPE_ERR      "group should be a valid group name or gid"
#define MSG_GROUP_NOPERM        "only super-user can setgid"

#define MSG_STDIN_TYPE_ERR      "stdin should be an valid file object"
#define MSG_STDIN_INVALID       "stdin should be readable by current user"

#define MSG_STDOUT_TYPE_ERR     "stdout should be a valid file object"
#define MSG_STDOUT_INVALID      "stdout should be writable by current user"

#define MSG_STDERR_TYPE_ERR     "stderr should be a valid file object"
#define MSG_STDERR_INVALID      "stdout should be writable by current user"

#define MSG_QUOTA_TYPE_ERR      "quota should be a list or dictionary of " \
                                "integers"
#define MSG_QUOTA_VAL_ERR       "quota value is invalid"
#define MSG_QUOTA_INVALID       "failed to access elements in quota list"

#define MSG_POLICY_TYPE_ERR     "policy should be a callable object"
#define MSG_POLICY_CALL_FAILED  "policy failed to determine action"

#define MSG_NO_IMPL             "method is not implemented"

#define MSG_ATTR_ADD_FAILED     "failed to add new attribute to module"
#define MSG_ALLOC_FAILED        "failed to allocate new object"
#define MSG_SBOX_CHECK_FAILED   "failed in sandbox pre-execution check"
#define MSG_CTRL_CHECK_FAILED   "failed in sandbox control policy check"
#define MSG_TYPE_READY_FAILED   "failed to finalize new type object"
#define MSG_TYPE_ADD_FAILED     "failed to add new type object to module"

#define MSG_PROBE_NOT_STARTED   "cannot probe a sandbox that is not started"

#define MSG_DUMP_NOT_BLOCKED    "cannot dump a sandbox unless it is blocked"
#define MSG_DUMP_PROBE_FAILED   "failed to probe the sandbox"
#define MSG_DUMP_DUMP_FAILED    "failed to dump the sandbox"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_MODULE_H__ */
