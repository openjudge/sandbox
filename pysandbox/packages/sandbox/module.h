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

#include <sandbox.h>
#include <Python.h>
#include <structmember.h>

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

#ifdef __cplusplus
}
#endif

#define MSG_ARGS_TOO_LONG       "command line too long"
#define MSG_ARGS_TYPE_ERR       "command line should be a string or list of " \
                                "strings"
#define MSG_ARGS_VAL_ERR        "command line should be a string or list of " \
                                "strings"
#define MSG_ARGS_INVALID        "command line should contain full path to an " \
                                "executable"

#define MSG_JAIL_TOO_LONG       "program jail too long"
#define MSG_JAIL_TYPE_ERR       "program jail should be a string"
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
#define MSG_QUOTA_INVALID       "failed accessing element from quota list"

#define MSG_POLICY_TYPE_ERR     "policy should be a callable object"
#define MSG_POLICY_CALL_FAILED  "policy failed to determine action"

#define MSG_NO_IMPL             "this method is not implemented"

#define MSG_ATTR_ADD_FAILED     "failed to add new attribute to module"
#define MSG_ALLOC_FAILED        "failed to allocate new object"
#define MSG_SBOX_CHECK_FAILED   "failed in sandbox pre-execution check"
#define MSG_CTRL_CHECK_FAILED   "failed in sandbox control policy check"
#define MSG_TYPE_READY_FAILED   "failed to finalize new type object"
#define MSG_TYPE_ADD_FAILED     "failed to add new type object to module"

#define MSG_PROB_NOT_STARTED    "cannot probe a sandbox that is not started"

#define MSG_DUMP_NOT_BLOCKED    "cannot dump a sandbox unless it is blocked"
#define MSG_DUMP_PROB_FAILED    "failed to probe the sandbox"
#define MSG_DUMP_DUMP_FAILED    "failed to dump the sandbox"

#endif /* __OJS_MODULE_H__ */
