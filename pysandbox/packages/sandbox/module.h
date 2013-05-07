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
#define PY3K
#else
#undef  PY3K
#endif

#ifndef PyDoc_STR
#define PyDoc_VAR(name)                 static char name[]
#define PyDoc_STR(doc)                  (doc)
#define PyDoc_STRVAR(name, doc)         PyDoc_VAR(name) = PyDoc_STR(doc)
#endif /* PyDoc_STR */

#ifndef Py_NULL
#define Py_NULL                         ((PyObject *)NULL)
#endif /* Py_NULL */

#ifdef __cplusplus
extern "C"
{
#endif

/* Native structures */

typedef struct
{
    PyObject_HEAD
    union
    {
        event_t event;
        action_t action;
        struct
        {
            PyObject * e;
            PyObject * a;
        } policy;
    } raw;
} Any;

#define Any_AS_EVENT(o)                 (((Any *)(o))->raw.event)
#define Any_AS_ACTION(o)                (((Any *)(o))->raw.action)
#define Any_AS_POLICY(o)                (((Any *)(o))->raw.policy)

typedef struct 
{
    Any head;
} SandboxEvent;

#define SandboxEvent_GET_EVENT(o)       Any_AS_EVENT((Any *)(o))

typedef struct 
{
    Any head;
} SandboxAction;

#define SandboxAction_GET_ACTION(o)     Any_AS_ACTION((Any *)(o))

typedef struct 
{
    Any head;
} SandboxPolicy;

#define SandboxPolicy_GET_STATE(o)      Any_AS_POLICY((Any *)(o))

typedef struct 
{
    Any head;
    sandbox_t sbox;
    struct
    {
        PyObject * i;
        PyObject * o;
        PyObject * e;
    } io;
} Sandbox;

#define Sandbox_GET_SBOX(o)             (((Sandbox *)(o))->sbox)
#define Sandbox_GET_IO(o)               (((Sandbox *)(o))->io)

/* Messages */

#ifdef PY3K

#define MSG_LONG_TYPE           "int"
#define MSG_STR_TYPES           "bytes or str"

#else /* Python 2 */

#define MSG_LONG_TYPE           "long"
#define MSG_STR_TYPES           "str or unicode"

#endif /* PY3K */

#define MSG_STR_OBJ             MSG_STR_TYPES " object"
#define MSG_STR_OR_SEQ          "str or sequence of str"
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

#define MSG_QUOTA_TYPE_ERR      "quota should be a list / tuple or dict of " \
                                MSG_LONG_TYPE " values"
#define MSG_QUOTA_VAL_ERR       "quota value is invalid"
#define MSG_QUOTA_INVALID       "failed to get quota value from list / tuple"

#define MSG_POLICY_TYPE_ERR     "policy should be an instance of SandboxPolicy"
#define MSG_POLICY_CALL_FAILED  "policy failed to determine action"
#define MSG_POLICY_DEL_FORBID   "policy should not be deleted"
#define MSG_POLICY_SET_FORBID   "cannot change policy when the sandboxed "\
                                "program is running"

#define MSG_NO_IMPL             "method is not implemented"

#define MSG_ATTR_ADD_FAILED     "failed to add new attribute to module"
#define MSG_ALLOC_FAILED        "failed to allocate new object"
#define MSG_SBOX_INIT_FAILED    "failed in sandbox initialization"
#define MSG_SBOX_CHECK_FAILED   "failed in sandbox pre-execution check"
#define MSG_CTRL_CHECK_FAILED   "failed in sandbox control policy check"
#define MSG_TYPE_ADD_FAILED     "failed to add new type object to module"
#define MSG_BLOCK_SIG_FAILED    "failed to block signals reserved by module"

#define MSG_PROBE_NOT_STARTED   "cannot probe a sandbox that is not started"

#define MSG_DUMP_NOT_BLOCKED    "cannot dump a sandbox unless it is blocked"
#define MSG_DUMP_PROBE_FAILED   "failed to probe the sandbox"
#define MSG_DUMP_TYPE_ERR       "typid is invalid for dump"
#define MSG_DUMP_INVALID        "address is invalid for dump"
#define MSG_DUMP_FAILED         "failed to dump the sandbox"


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_MODULE_H__ */
