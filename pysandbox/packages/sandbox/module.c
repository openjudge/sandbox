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

#include "module.h"
#include <bytesobject.h>       /* py3k forward compatibility */
#include <sandbox-dev.h>       /* libsandbox internals */
#include <pwd.h>               /* struct passwd, getpwnam(), getpwuid() */
#include <grp.h>               /* struct group, getgrnam(), getgrgid() */

#ifdef __cplusplus
extern "C"
{
#endif

/* module doc */

PyDoc_STRVAR(__doc__,
"The Sandbox Libraries (Python)");

/* anyType */

PyDoc_STRVAR(DOC_TP_ANY,        "");

static PyTypeObject anyType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_sandbox.Any",                             /* tp_name */
    sizeof(Any),                                /* tp_basicsize */
    0,                                          /* tp_itemsize */
    0,                                          /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    DOC_TP_ANY,                                 /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    0,                                          /* tp_new */
};

/* eventType */

PyDoc_STRVAR(DOC_TP_EVENT,      "");
PyDoc_STRVAR(DOC_EVENT_TYPE,    "");
PyDoc_STRVAR(DOC_EVENT_DATA,    "");

static int SandboxEvent_init(SandboxEvent *, PyObject *, PyObject *);
static void SandboxEvent_free(SandboxEvent *);

static PyMemberDef eventMembers[] = 
{
    {"type", T_INT, offsetof(Any, raw.event.type), READONLY, DOC_EVENT_TYPE},
    {"data", T_INT, offsetof(Any, raw.event.data), READONLY, DOC_EVENT_DATA},
/* In 32bit systems, the width of each native field in raw.data is 32 bits, and 
 * both 'data' and 'ext0' interpret the first 32 bits at the beginning of 
 * raw.data as a 32bit int; in 64bit systems, 'data' still represents the first 
 * 32bit int, whereas 'ext0' represents the second 32bit int of raw.data */
    {"ext0", T_INT, offsetof(Any, raw.event.data.__bitmap__.A) + 
     sizeof(long) - sizeof(int), READONLY, NULL},
    {"ext1", T_LONG, offsetof(Any, raw.event.data.__bitmap__.B), READONLY, 
     NULL},
    {"ext2", T_LONG, offsetof(Any, raw.event.data.__bitmap__.C), READONLY, 
     NULL},
    {"ext3", T_LONG, offsetof(Any, raw.event.data.__bitmap__.D), READONLY, 
     NULL},
    {"ext4", T_LONG, offsetof(Any, raw.event.data.__bitmap__.E), READONLY, 
     NULL},
    {"ext5", T_LONG, offsetof(Any, raw.event.data.__bitmap__.F), READONLY, 
     NULL},
    {"ext6", T_LONG, offsetof(Any, raw.event.data.__bitmap__.G), READONLY, 
     NULL},
    {NULL, 0, 0, 0, NULL}       /* Sentinel */
};

static PyTypeObject eventType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_sandbox.SandboxEvent",                    /* tp_name */
    sizeof(SandboxEvent),                       /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)SandboxEvent_free,              /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    DOC_TP_EVENT,                               /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    eventMembers,                               /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)SandboxEvent_init,                /* tp_init */
    0,                                          /* tp_alloc */
    0,                                          /* tp_new */
};

static int 
SandboxEvent_init(SandboxEvent * self, PyObject * args, PyObject * kwds)
{
    FUNC_BEGIN("%p,%p,%p", self, args, kwds);
    assert(self && args);
    
    memset(&SandboxEvent_GET_EVENT(self), 0, sizeof(event_t));
    
    if (!PyArg_ParseTuple(args, "i|lllllll",
        &SandboxEvent_GET_EVENT(self).type,
        &SandboxEvent_GET_EVENT(self).data.__bitmap__.A,
        &SandboxEvent_GET_EVENT(self).data.__bitmap__.B,
        &SandboxEvent_GET_EVENT(self).data.__bitmap__.C,
        &SandboxEvent_GET_EVENT(self).data.__bitmap__.D,
        &SandboxEvent_GET_EVENT(self).data.__bitmap__.E,
        &SandboxEvent_GET_EVENT(self).data.__bitmap__.F,
        &SandboxEvent_GET_EVENT(self).data.__bitmap__.G))
    {
        FUNC_RET("%d", -1);
    }
    
    FUNC_RET("%d", 0);
}

static void
SandboxEvent_free(SandboxEvent * self)
{
    PROC_BEGIN("%p", self);
    assert(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
    PROC_END();
}

static int
SandboxEvent_Check(const PyObject * o)
{
    FUNC_BEGIN("%p", o);
    if (o == NULL)
    {
        FUNC_RET("%d", 0);
    }
    FUNC_RET("%d", PyObject_TypeCheck(o, &eventType));
}

static PyObject * 
SandboxEvent_SET_EVENT(PyObject * o, const event_t * pevent)
{
    FUNC_BEGIN("%p,%p", o, pevent);
    if (pevent != NULL)
    {
        memcpy(&SandboxEvent_GET_EVENT(o), pevent, sizeof(event_t));
    }
    else
    {
        memset(&SandboxEvent_GET_EVENT(o), 0, sizeof(event_t));
    }
    FUNC_RET("%p", o);
}

static PyObject *
SandboxEvent_FromEvent(const event_t * pevent)
{
    FUNC_BEGIN("%p", pevent);
    PyObject * e = eventType.tp_alloc(&eventType, 0);
    if (e == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        FUNC_RET("%p", Py_NULL);
    }
    SandboxEvent_SET_EVENT(e, pevent);
    FUNC_RET("%p", e);
}

/* actionType */

PyDoc_STRVAR(DOC_TP_ACTION,     "");
PyDoc_STRVAR(DOC_ACTION_TYPE,   "");
PyDoc_STRVAR(DOC_ACTION_DATA,   "");

static int SandboxAction_init(SandboxAction *, PyObject *, PyObject *);
static void SandboxAction_free(SandboxAction *);

static PyMemberDef actionMembers[] = 
{
    {"type", T_INT, offsetof(Any, raw.action.type), RESTRICTED, DOC_ACTION_TYPE},
    {"data", T_INT, offsetof(Any, raw.action.data), RESTRICTED, DOC_ACTION_DATA},
    {NULL, 0, 0, 0, NULL}       /* Sentinel */
};

static PyTypeObject actionType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_sandbox.SandboxAction",                   /* tp_name */
    sizeof(SandboxAction),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)SandboxAction_free,             /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    DOC_TP_ACTION,                              /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    actionMembers,                              /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)SandboxAction_init,               /* tp_init */
    0,                                          /* tp_alloc */
    0,                                          /* tp_new */
};

static int 
SandboxAction_init(SandboxAction * self, PyObject * args, PyObject * kwds)
{
    FUNC_BEGIN("%p,%p,%p", self, args, kwds);
    assert(self && args);
    
    memset(&SandboxAction_GET_ACTION(self), 0, sizeof(action_t));
    
    if (!PyArg_ParseTuple(args, "i|ll", &SandboxAction_GET_ACTION(self).type,
        &SandboxAction_GET_ACTION(self).data.__bitmap__.A,
        &SandboxAction_GET_ACTION(self).data.__bitmap__.B))
    {
        FUNC_RET("%d", -1);
    }
    
    FUNC_RET("%d", 0);
}

static void
SandboxAction_free(SandboxAction * self)
{
    PROC_BEGIN("%p", self);
    assert(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
    PROC_END();
}

static int
SandboxAction_Check(const PyObject * o)
{
    FUNC_BEGIN("%p", o);
    if (o == NULL)
    {
        FUNC_RET("%d", 0);
    }
    FUNC_RET("%d", PyObject_TypeCheck(o, &actionType));
}

static PyObject *
SandboxAction_SET_ACTION(PyObject * o, const action_t * paction)
{
    FUNC_BEGIN("%p,%p", o, paction);
    if (paction != NULL)
    {
        memcpy(&SandboxAction_GET_ACTION(o), paction, sizeof(action_t));
    }
    else
    {
        memset(&SandboxAction_GET_ACTION(o), 0, sizeof(action_t));
    }
    FUNC_RET("%p", o);
}

static PyObject *
SandboxAction_FromAction(const action_t * paction)
{
    FUNC_BEGIN("%p", paction);
    PyObject * a = actionType.tp_alloc(&actionType, 0);
    if (a == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        FUNC_RET("%p", Py_NULL);
    }
    SandboxAction_SET_ACTION(a, paction);
    FUNC_RET("%p", a);
}

/* policyType */

PyDoc_STRVAR(DOC_TP_POLICY,     "");

static int SandboxPolicy_init(SandboxPolicy *, PyObject *, PyObject *);
static void SandboxPolicy_free(SandboxPolicy *);
static PyObject * SandboxPolicy_call(SandboxPolicy *, PyObject *, PyObject *);

static PyTypeObject policyType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_sandbox.SandboxPolicy",                   /* tp_name */
    sizeof(SandboxPolicy),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)SandboxPolicy_free,             /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    (ternaryfunc)SandboxPolicy_call,            /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    DOC_TP_POLICY,                              /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)SandboxPolicy_init,               /* tp_init */
    0,                                          /* tp_alloc */
    0,                                          /* tp_new */
};

static int 
SandboxPolicy_init(SandboxPolicy * self, PyObject * args, PyObject * kwds)
{
    FUNC_BEGIN("%p,%p,%p", self, args, kwds);
    assert(self && args);
    SandboxPolicy_GET_STATE(self).e = NULL;
    SandboxPolicy_GET_STATE(self).a = NULL;
    FUNC_RET("%d", 0);
}

static void
SandboxPolicy_free(SandboxPolicy * self)
{
    PROC_BEGIN("%p", self);
    assert(self);
    Py_XDECREF(SandboxPolicy_GET_STATE(self).e);
    SandboxPolicy_GET_STATE(self).e = NULL;
    Py_XDECREF(SandboxPolicy_GET_STATE(self).a);
    SandboxPolicy_GET_STATE(self).a = NULL;
    Py_TYPE(self)->tp_free((PyObject *)self);
    PROC_END();
}

static PyObject *
SandboxPolicy_call(SandboxPolicy * self, PyObject * args, PyObject * kwds)
{
    FUNC_BEGIN("%p,%p,%p", self, args, kwds);
    assert(self && args);
    
    PyObject * e = NULL;
    PyObject * a = NULL;
    
    if (!PyArg_UnpackTuple(args, "__call__", 2, 2, &e, &a))
    {
        FUNC_RET("%p", Py_NULL);
    }
    
    if ((e == NULL) || ((e != SandboxPolicy_GET_STATE(self).e) && 
        !SandboxEvent_Check(e)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_POLICY_CALL_FAILED);
        FUNC_RET("%p", Py_NULL);
    }
    
    if ((a == NULL) || ((a != SandboxPolicy_GET_STATE(self).a) && 
        !SandboxAction_Check(a)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_POLICY_CALL_FAILED);
        FUNC_RET("%p", Py_NULL);
    }
    
    /* Since 0.3.3-rc4, the core library exports a baseline policy function
     * sandbox_default_policy() for preliminary (minimal black list) sandboxing.
     * The calling programs of the core library, i.e. pysandbox, no longer need 
     * to include local baseline policies. They should, of course, override the 
     * baseline policy if more sophisticated sandboxing rules are desired. */
    
    sandbox_default_policy(NULL, &SandboxEvent_GET_EVENT(e), 
        &SandboxAction_GET_ACTION(a));
    
    Py_INCREF(a);
    
    FUNC_RET("%p", a);
}

static int
SandboxPolicy_Check(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    if (o == NULL)
    {
        FUNC_RET("%d", 0);
    }
    FUNC_RET("%d", PyObject_TypeCheck(o, &policyType));
}

static PyObject *
SandboxPolicy_New(void)
{
    FUNC_BEGIN();
    PyObject * args = Py_BuildValue("()");
    PyObject * o = policyType.tp_new(&policyType, args, NULL);
    Py_DECREF(args);
    FUNC_RET("%p", o);
}

static void 
SandboxPolicy_entry(const policy_t * ppolicy, const event_t * pevent, 
                     action_t * paction)
{
    PROC_BEGIN("%p,%p,%p", ppolicy, pevent, paction);
    
    assert(ppolicy && pevent && paction);
    
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    
    SandboxPolicy * p = (SandboxPolicy *)ppolicy->data;
    if (!SandboxPolicy_Check((PyObject *)p))
    {
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_BP}}};
        PyGILState_Release(gstate);
        PROC_END();
    }
    
    (SandboxPolicy_GET_STATE(p).e) ? 
        SandboxEvent_SET_EVENT(SandboxPolicy_GET_STATE(p).e, pevent) :
        (SandboxPolicy_GET_STATE(p).e = SandboxEvent_FromEvent(pevent));
    
    (SandboxPolicy_GET_STATE(p).a) ? 
        SandboxAction_SET_ACTION(SandboxPolicy_GET_STATE(p).a, NULL) :
        (SandboxPolicy_GET_STATE(p).a = SandboxAction_FromAction(NULL));
    
    PyObject * o = PyObject_CallFunctionObjArgs((PyObject *)p, 
        SandboxPolicy_GET_STATE(p).e, SandboxPolicy_GET_STATE(p).a, NULL);
    
    if ((o == NULL) || PyErr_Occurred() || ((o != SandboxPolicy_GET_STATE(p).a) 
        && !SandboxAction_Check(o)))
    {
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_BP}}};
    }
    else
    {
        *paction = SandboxAction_GET_ACTION(o);
    }
    
    Py_XDECREF(o);
    
    PyGILState_Release(gstate);
    
    PROC_END();
}

#ifndef SandboxPolicy_AS_POLICY
#define SandboxPolicy_AS_POLICY(o) \
    ((policy_t){(void *)SandboxPolicy_entry, (long)(o)})
#endif /* SandboxPolicy_AS_POLICY */

/* sandboxType */

PyDoc_STRVAR(DOC_TP_SANDBOX,    "");

PyDoc_STRVAR(DOC_SANDBOX_STATUS, 
"runtime state of the sandbox instance (can be any of S_STATUS_*)");

PyDoc_STRVAR(DOC_SANDBOX_RESULT, 
"final state of the sandboxed program (can be any of S_RESULT_*)");

PyDoc_STRVAR(DOC_SANDBOX_JAIL, 
"chroot jail path (str) of the sandboxed program");

PyDoc_STRVAR(DOC_SANDBOX_TASK, 
"command line arguments (tuple of str) of the sandboxed program");

PyDoc_STRVAR(DOC_SANDBOX_QUOTA, 
"quota limits (tuple of " MSG_LONG_TYPE ") of the sandboxed program");

PyDoc_STRVAR(DOC_SANDBOX_POLICY, 
"policy object (instance of SandboxPolicy) of the sandbox instance");

PyDoc_STRVAR(DOC_SANDBOX_UID, 
"user id (int) as whom to launch the sandboxed program");

PyDoc_STRVAR(DOC_SANDBOX_GID, 
"group id (int) as whom to launch the sandboxed program");

PyDoc_STRVAR(DOC_SANDBOX_PID, 
"process id (int) of the sandboxed program, or None if not running");

PyDoc_STRVAR(DOC_SANDBOX_DUMP,  "");

PyDoc_STRVAR(DOC_SANDBOX_PROBE, "");

PyDoc_STRVAR(DOC_SANDBOX_RUN,   "");

static PyMemberDef sandboxMembers[] = 
{
    {"owner", T_INT, offsetof(Sandbox, sbox.task.uid), READONLY, 
     DOC_SANDBOX_UID},
    {"group", T_INT, offsetof(Sandbox, sbox.task.gid), READONLY, 
     DOC_SANDBOX_GID},
    {NULL, 0, 0, 0, NULL}       /* Sentinel */
};

static PyObject * Sandbox_get_pid(Sandbox *, void *);
static PyObject * Sandbox_get_task(Sandbox *, void *);
static PyObject * Sandbox_get_jail(Sandbox *, void *);
static PyObject * Sandbox_get_quota(Sandbox *, void *);
static PyObject * Sandbox_get_policy(Sandbox *, void *);
static PyObject * Sandbox_get_status(Sandbox *, void *);
static PyObject * Sandbox_get_result(Sandbox *, void *);
static int Sandbox_set_policy(Sandbox *, PyObject *, void *);

static PyGetSetDef sandboxGetSetters[] = 
{
    {"jail", (getter)Sandbox_get_jail, 0, DOC_SANDBOX_JAIL, NULL}, 
    {"task", (getter)Sandbox_get_task, 0, DOC_SANDBOX_TASK, NULL}, 
    {"quota", (getter)Sandbox_get_quota, 0, DOC_SANDBOX_QUOTA, NULL}, 
    {"policy", (getter)Sandbox_get_policy, (setter)Sandbox_set_policy, 
     DOC_SANDBOX_POLICY, NULL}, 
    {"status", (getter)Sandbox_get_status, 0, DOC_SANDBOX_STATUS, NULL}, 
    {"result", (getter)Sandbox_get_result, 0, DOC_SANDBOX_RESULT, NULL}, 
    {"pid", (getter)Sandbox_get_pid, 0, DOC_SANDBOX_PID, NULL}, 
    {NULL, 0, 0, 0, NULL}       /* Sentinel */
};

static PyObject * Sandbox_run(Sandbox *);
static PyObject * Sandbox_probe(Sandbox *);
static PyObject * Sandbox_dump(Sandbox *, PyObject *);

static PyMethodDef sandboxMethods[] = 
{
    {"dump", (PyCFunction)Sandbox_dump, METH_VARARGS, DOC_SANDBOX_DUMP},
    {"probe", (PyCFunction)Sandbox_probe, METH_NOARGS, DOC_SANDBOX_PROBE},
    {"run", (PyCFunction)Sandbox_run, METH_NOARGS, DOC_SANDBOX_RUN},
    {NULL, NULL, 0, NULL}       /* Sentinel */
};

static PyObject * Sandbox_new(PyTypeObject *, PyObject *, PyObject *);
static void Sandbox_free(Sandbox *);
static int Sandbox_traverse(Sandbox *, visitproc, void *);
static int Sandbox_clear(Sandbox *);

static PyTypeObject sandboxType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_sandbox.Sandbox",                         /* tp_name */
    sizeof(Sandbox),                            /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)Sandbox_free,                   /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE 
                       | Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    DOC_TP_SANDBOX,                             /* tp_doc */
    (traverseproc)Sandbox_traverse,             /* tp_traverse */
    (inquiry)Sandbox_clear,                     /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    sandboxMethods,                             /* tp_methods */
    sandboxMembers,                             /* tp_members */
    sandboxGetSetters,                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    Sandbox_new,                                /* tp_new */
};

static int Sandbox_load_comm(PyObject *, Sandbox *);
static int Sandbox_load_jail(PyObject *, Sandbox *);
static int Sandbox_load_uid(PyObject *, Sandbox *);
static int Sandbox_load_gid(PyObject *, Sandbox *);
static int Sandbox_load_ifd(PyObject *, Sandbox *);
static int Sandbox_load_ofd(PyObject *, Sandbox *);
static int Sandbox_load_efd(PyObject *, Sandbox *);
static int Sandbox_load_quota(PyObject *, Sandbox *);
static int Sandbox_load_policy(PyObject *, Sandbox *);

static PyObject *
Sandbox_new(PyTypeObject * type, PyObject * args, PyObject * kwds)
{
    FUNC_BEGIN("%p,%p,%p", type, args, kwds);
    assert(type && args);
    
    Sandbox * self = (Sandbox *)type->tp_alloc(type, 0);
    if (self == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        FUNC_RET("%p", Py_NULL);
    }
    
    if (sandbox_init(&Sandbox_GET_SBOX(self), NULL) != 0)
    {
        Py_DECREF((PyObject *)self);
        PyErr_SetString(PyExc_RuntimeError, MSG_SBOX_INIT_FAILED);
        FUNC_RET("%p", Py_NULL);
    }
    
    memset(&Sandbox_GET_IO(self), 0, sizeof(Sandbox_GET_IO(self)));
    
    PyObject * p = SandboxPolicy_New();
    if (p == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        Py_DECREF((PyObject *)self);
        FUNC_RET("%p", Py_NULL);
    }
    if (Sandbox_set_policy(self, p, NULL) != 0)
    {
        Py_DECREF((PyObject *)self);
        Py_DECREF(p);
        FUNC_RET("%p", Py_NULL);
    }
    Py_DECREF(p);
    
    /* Sandbox instance is (almost) immutable (except for attribute policy), so
     * argument parsing is in __new__() rather than in __init__() */
    
    static char * keywords[] = {
        "args",                 /* Command line arguments */
        "jail",                 /* Program jail directory */
        "owner",                /* Owner name / id */
        "group",                /* Group name / id */
        "stdin",                /* Input channel */
        "stdout",               /* Output channel */
        "stderr",               /* Error channel */
        "quota",                /* Resource quota */
        "policy",               /* Sandbox control policy */
        NULL                    /* Sentinel */
    };
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, 
        "O&|O&O&O&O&O&O&O&O&", keywords, 
        Sandbox_load_comm, self, 
        Sandbox_load_jail, self, 
        Sandbox_load_uid, self, 
        Sandbox_load_gid, self, 
        Sandbox_load_ifd, self, 
        Sandbox_load_ofd, self, 
        Sandbox_load_efd, self, 
        Sandbox_load_quota, self,
        Sandbox_load_policy, self))
    {
        Py_DECREF((PyObject *)self);
        FUNC_RET("%p", Py_NULL);
    }
    
    if (!sandbox_check(&Sandbox_GET_SBOX(self)))
    {
        Py_DECREF((PyObject *)self);
        PyErr_SetString(PyExc_AssertionError, MSG_SBOX_CHECK_FAILED);
        FUNC_RET("%p", Py_NULL);
    }
    
    FUNC_RET("%p", (PyObject *)self);
}

static void
Sandbox_free(Sandbox * self)
{
    PROC_BEGIN("%p", self);
    assert(self);
    Sandbox_clear(self);
    sandbox_fini(&Sandbox_GET_SBOX(self));
    Py_TYPE(self)->tp_free((PyObject *)self);
    PROC_END();
}

static int 
Sandbox_traverse(Sandbox * self, visitproc visit, void * arg)
{
    FUNC_BEGIN("%p,%p,%p", self, visit, arg);
    
    Py_VISIT(Sandbox_GET_IO(self).i);
    Py_VISIT(Sandbox_GET_IO(self).o);
    Py_VISIT(Sandbox_GET_IO(self).e);
    
    int res = 0;
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    PyObject * policy = (PyObject *)Sandbox_GET_SBOX(self).ctrl.policy.data;
    if (SandboxPolicy_Check(policy))
    {
        res = visit(policy, arg);
    }
    else if (policy != NULL)
    {
        WARN("have no idea what kind of policy it was");
    }
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", res);
}

static int
Sandbox_CLEAR_POLICY(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    
    PyObject * policy = (PyObject *)Sandbox_GET_SBOX(self).ctrl.policy.data;
    if (SandboxPolicy_Check(policy))
    {
        Py_DECREF(policy);
    }
    else if (policy != NULL)
    {
        WARN("have no idea what kind of policy it was");
    }
    Sandbox_GET_SBOX(self).ctrl.policy = \
        (policy_t){(void *)sandbox_default_policy, 0};
    
    FUNC_RET("%d", 0);
}

static int
Sandbox_clear(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    Sandbox_CLEAR_POLICY(self);
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    Py_CLEAR(Sandbox_GET_IO(self).i);
    Py_CLEAR(Sandbox_GET_IO(self).o);
    Py_CLEAR(Sandbox_GET_IO(self).e);
    
    FUNC_RET("%d", 0);
}

static int
Integer_Check(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    assert(o);
    
    if (PyLong_Check(o))
    {
        FUNC_RET("%d", 1);
    }
#ifdef PY3K
    /* long() and int() types are unified in py3k */
#else
    if (PyInt_Check(o))
    {
        FUNC_RET("%d", 1);
    }
#endif
    FUNC_RET("%d", 0);
}

static PyObject *
UTF8Bytes_FromObject(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    assert(o);
    
    if (PyBytes_Check(o))
    {
        Py_INCREF(o);
        FUNC_RET("%p", o);
    }
    
    if (PyUnicode_Check(o))
    {
        FUNC_RET("%p", PyUnicode_AsUTF8String(o));
    }
    
    PyErr_SetString(PyExc_TypeError, MSG_STR_TYPE_ERR);
    FUNC_RET("%p", Py_NULL);
}

static int
Sandbox_load_comm(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    command_t target;
    memset(&target, 0, sizeof(command_t));
    
    size_t argc = 0;
    size_t offset = 0;
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        PyObject * pyutf8 = UTF8Bytes_FromObject(o);
        if (pyutf8 == NULL)
        {
            FUNC_RET("%d", 0);
        }
        size_t delta = PyBytes_GET_SIZE(pyutf8) + 1;
        if (offset + delta < sizeof(target.buff))
        {
            strcpy(target.buff, PyBytes_AS_STRING(pyutf8));
            target.args[argc++] = offset;
            offset += delta;
            Py_DECREF(pyutf8);
        }
        else
        {
            Py_DECREF(pyutf8);
            PyErr_SetString(PyExc_OverflowError, MSG_ARGS_TOO_LONG);
            FUNC_RET("%d", 0);
        }
        target.buff[offset] = '\0';
        target.args[argc] = -1;
    }
    else if (PySequence_Check(o))
    {
        Py_ssize_t sz = PySequence_Size(o);
        while ((sz > 0) && (argc < (size_t)sz))
        {
            PyObject * item = PySequence_GetItem(o, argc);
            if (PyBytes_Check(item) || PyUnicode_Check(item))
            {
                item = UTF8Bytes_FromObject(item);
                if (item == NULL)
                {
                    FUNC_RET("%d", 0);
                    break;
                }
                size_t delta  = PyBytes_GET_SIZE(item) + 1;
                if (offset + delta >= sizeof(target.buff))
                {
                    Py_DECREF(item);
                    PyErr_SetString(PyExc_OverflowError, MSG_ARGS_TOO_LONG);
                    FUNC_RET("%d", 0);
                    break;
                }
                if (argc + 1 >= sizeof(target.args) / sizeof(int))
                {
                    Py_DECREF(item);
                    PyErr_SetString(PyExc_OverflowError, MSG_ARGS_TOO_LONG);
                    FUNC_RET("%d", 0);
                    break;
                }
                strcpy(target.buff + offset, PyBytes_AS_STRING(item));
                target.args[argc++] = offset;
                offset += delta;
                Py_DECREF(item);
            }
            else
            {
                PyErr_SetString(PyExc_TypeError, MSG_ARGS_TYPE_ERR);
                FUNC_RET("%d", 0);
            }
        }
        target.buff[offset] = '\0';
        target.args[argc] = -1;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_ARGS_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    if (offset == 0)
    {
        PyErr_SetString(PyExc_ValueError, MSG_ARGS_VAL_ERR);
        FUNC_RET("%d", 0);
    }
    
    struct stat s;
    if ((stat(target.buff, &s) < 0) || !S_ISREG(s.st_mode))
    {
        PyErr_SetString(PyExc_ValueError, MSG_ARGS_INVALID);
        FUNC_RET("%d", 0);
    }
    
    if (!((S_IXUSR & s.st_mode) && (s.st_uid == Sandbox_GET_SBOX(self).task.uid)) && 
        !((S_IXGRP & s.st_mode) && (s.st_gid == Sandbox_GET_SBOX(self).task.gid)) && 
        !(S_IXOTH & s.st_mode) && !(Sandbox_GET_SBOX(self).task.uid == (uid_t)0))
    {
        PyErr_SetString(PyExc_ValueError, MSG_ARGS_INVALID);
        FUNC_RET("%d", 0);
    }
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    memcpy(&Sandbox_GET_SBOX(self).task.comm, &target, sizeof(command_t));
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_load_jail(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    char target[SBOX_PATH_MAX] = {'/', '\0'};
    
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        PyObject * pyutf8 = UTF8Bytes_FromObject(o);
        if (pyutf8 == NULL)
        {
            FUNC_RET("%d", 0);
        }
        if (PyBytes_GET_SIZE(pyutf8) + 1 < (Py_ssize_t)sizeof(target))
        {
            strcpy(target, PyBytes_AS_STRING(pyutf8));
            Py_DECREF(pyutf8);
        }
        else
        {
            Py_DECREF(pyutf8);
            PyErr_SetString(PyExc_OverflowError, MSG_JAIL_TOO_LONG);
            FUNC_RET("%d", 0);
        }
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_JAIL_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    if (strcmp(target, "/") != 0)
    {
        /* only super-user can chroot */
        if (getuid() != (uid_t)0)
        {
            PyErr_SetString(PyExc_AssertionError, MSG_JAIL_NOPERM);
            FUNC_RET("%d", 0);
        }
        
        if (access(target, X_OK | R_OK) < 0)
        {
            PyErr_SetString(PyExc_ValueError, MSG_JAIL_INVALID);
            FUNC_RET("%d", 0);
        }
        
        struct stat s;
        if ((stat(target, &s) < 0) || !S_ISDIR(s.st_mode))
        {
            PyErr_SetString(PyExc_ValueError, MSG_JAIL_INVALID);
            FUNC_RET("%d", 0);
        }
    }
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    memcpy(Sandbox_GET_SBOX(self).task.jail, target, sizeof(target));
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_load_uid(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    uid_t uid = getuid();

    struct passwd * pw = NULL;
    
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        PyObject * pyutf8 = UTF8Bytes_FromObject(o);
        if (pyutf8 == NULL)
        {
            FUNC_RET("%d", 0);
        }
        if ((pw = getpwnam(PyBytes_AS_STRING(pyutf8))) != NULL)
        {
            uid = pw->pw_uid;
            Py_DECREF(pyutf8);
        }
        else
        {
            Py_DECREF(pyutf8);
            PyErr_SetString(PyExc_ValueError, MSG_OWNER_MISSING);
            FUNC_RET("%d", 0);
        }
    }
    else if (Integer_Check(o))
    {
        PyObject * pylong = PyNumber_Long(o);
        uid_t temp = (uid_t)PyLong_AsLong(pylong);
        Py_XDECREF(pylong);
        if ((pw = getpwuid(temp)) == NULL)
        {
            PyErr_SetString(PyExc_ValueError, MSG_OWNER_MISSING);
            FUNC_RET("%d", 0);
        }
        uid = pw->pw_uid;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_OWNER_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    /* only super-user can setuid */
    if ((getuid() != (uid_t)0) && (getuid() != uid))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_OWNER_NOPERM);
        FUNC_RET("%d", 0);
    }
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    Sandbox_GET_SBOX(self).task.uid = uid;
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_load_gid(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    gid_t gid = getgid();
    
    struct group * gr = NULL;
    
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        PyObject * pyutf8 = UTF8Bytes_FromObject(o);
        if (pyutf8 == NULL)
        {
            FUNC_RET("%d", 0);
        }
        if ((gr = getgrnam(PyBytes_AS_STRING(pyutf8))) != NULL)
        {
            gid = gr->gr_gid;
            Py_DECREF(pyutf8);
        }
        else
        {
            Py_DECREF(pyutf8);
            PyErr_SetString(PyExc_ValueError, MSG_GROUP_MISSING);
            FUNC_RET("%d", 0);
        }
    }
    else if (Integer_Check(o))
    {
        PyObject * pylong = PyNumber_Long(o);
        gid_t temp = (gid_t)PyLong_AsLong(pylong);
        Py_XDECREF(pylong);
        if ((gr = getgrgid(temp)) == NULL)
        {
            PyErr_SetString(PyExc_ValueError, MSG_GROUP_MISSING);
            FUNC_RET("%d", 0);
        }
        gid = gr->gr_gid;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_GROUP_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    /* only super-user can setgid */
    if ((getuid() != (uid_t)0) && (getgid() != gid))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_GROUP_NOPERM);
        FUNC_RET("%d", 0);
    }
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    Sandbox_GET_SBOX(self).task.gid = gid;
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_load_ifd(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    int ifd = PyObject_AsFileDescriptor(o);

    if (PyErr_Occurred())
    {
        FUNC_RET("%d", 0);
    }
    
    if (ifd < 0)
    {
        PyErr_SetString(PyExc_TypeError, MSG_STDIN_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    struct stat s;
    if ((fstat(ifd, &s) < 0) || !(S_ISCHR(s.st_mode) || 
         S_ISREG(s.st_mode) || S_ISFIFO(s.st_mode)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDIN_INVALID);
        FUNC_RET("%d", 0);
    }
    
    if (!((S_IRUSR & s.st_mode) && (s.st_uid == getuid())) && 
        !((S_IRGRP & s.st_mode) && (s.st_gid == getgid())) && 
        !(S_IROTH & s.st_mode) && !(getuid() == (uid_t)0))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDIN_INVALID);
        FUNC_RET("%d", 0);
    }
    
    Py_XDECREF(Sandbox_GET_IO(self).i);
    Py_INCREF(Sandbox_GET_IO(self).i = o);
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    Sandbox_GET_SBOX(self).task.ifd = ifd;
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_load_ofd(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    int ofd = PyObject_AsFileDescriptor(o);
    
    if (PyErr_Occurred())
    {
        FUNC_RET("%d", 0);
    }
    
    if (ofd < 0)
    {
        PyErr_SetString(PyExc_TypeError, MSG_STDOUT_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    struct stat s;
    
    if ((fstat(ofd, &s) < 0) || !(S_ISCHR(s.st_mode) ||
         S_ISREG(s.st_mode) || S_ISFIFO(s.st_mode)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDOUT_INVALID);
        FUNC_RET("%d", 0);
    }
    
    if (!((S_IWUSR & s.st_mode) && (s.st_uid == getuid())) &&
        !((S_IWGRP & s.st_mode) && (s.st_gid == getgid())) &&
        !(S_IWOTH & s.st_mode) && !(getuid() == (uid_t)0))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDOUT_INVALID);
        FUNC_RET("%d", 0);
    }
    
    Py_XDECREF(Sandbox_GET_IO(self).o);
    Py_INCREF(Sandbox_GET_IO(self).o = o);
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    Sandbox_GET_SBOX(self).task.ofd = ofd;
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_load_efd(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    int efd = PyObject_AsFileDescriptor(o);
    
    if (PyErr_Occurred())
    {
        FUNC_RET("%d", 0);
    }
    
    if (efd < 0)
    {
        PyErr_SetString(PyExc_TypeError, MSG_STDERR_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    struct stat s;
    
    if ((fstat(efd, &s) < 0) || !(S_ISCHR(s.st_mode) ||
         S_ISREG(s.st_mode) || S_ISFIFO(s.st_mode)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDERR_INVALID);
        FUNC_RET("%d", 0);
    }
    
    if (!((S_IWUSR & s.st_mode) && (s.st_uid == getuid())) &&
        !((S_IWGRP & s.st_mode) && (s.st_gid == getgid())) &&
        !(S_IWOTH & s.st_mode) && !(getuid() == (uid_t)0))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDERR_INVALID);
        FUNC_RET("%d", 0);
    }
    
    Py_XDECREF(Sandbox_GET_IO(self).e);
    Py_INCREF(Sandbox_GET_IO(self).e = o);
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    Sandbox_GET_SBOX(self).task.efd = efd;
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static res_t
SandboxQuota_FromObject(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    
    PyObject * pyval = NULL;
    
    if (Integer_Check(o))
    {
        pyval = PyNumber_Long(o);
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_QUOTA_TYPE_ERR);
        FUNC_RET("%llu", (unsigned long long)RES_INFINITY);
    }
    
    long long val = PyLong_AsLongLong(pyval);
    
    Py_XDECREF(pyval);
    
    if ((val > RES_INFINITY) || (val < 0))
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_ValueError, MSG_QUOTA_VAL_ERR);
        }
        FUNC_RET("%llu", (unsigned long long)RES_INFINITY);
    }
    
    FUNC_RET("%llu", (unsigned long long)(res_t)val);
}

static int
Sandbox_load_quota(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    
    res_t quota[QUOTA_TOTAL];
   
    LOCK(&Sandbox_GET_SBOX(self), SH);
    memcpy(quota, Sandbox_GET_SBOX(self).task.quota, sizeof(quota));
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    const char * keywords[] = 
    {
        "wallclock",
        "cpu",
        "memory",
        "disk",
        NULL                    /* Sentinel */
    };
    
    Py_ssize_t t = 0;
    
    if (PyDict_Check(o))
    {
        for (t = 0; (t < QUOTA_TOTAL) && !PyErr_Occurred(); t++)
        {
            PyObject * value = NULL;
            if ((value = PyDict_GetItemString(o, (char *)keywords[t])) == NULL)
            {
                /* keep this quota[t] intact */
                continue;
            }
            quota[t] = SandboxQuota_FromObject(value);
        }
    }
    else if (PySequence_Check(o))
    {
        for (t = 0; (t < QUOTA_TOTAL) && !PyErr_Occurred(); t++)
        {
            if (t >= PySequence_Size(o))
            {
                /* keep this quota[t] intact */
                continue;
            }
            PyObject * value = NULL;
            if ((value = PySequence_GetItem(o, t)) == NULL)
            {
                PyErr_SetString(PyExc_IndexError, MSG_QUOTA_INVALID);
                break;
            }
            quota[t] = SandboxQuota_FromObject(value);
        }
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_QUOTA_TYPE_ERR);
    }
    
    if (PyErr_Occurred())
    {
        FUNC_RET("%d", 0);
    }
    
    LOCK(&Sandbox_GET_SBOX(self), EX);
    memcpy(Sandbox_GET_SBOX(self).task.quota, quota, sizeof(quota));
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_load_policy(PyObject * o, Sandbox * self)
{
    FUNC_BEGIN("%p,%p", o, self);
    assert(o && self);
    if (Sandbox_set_policy(self, o, NULL) != 0)
    {
        FUNC_RET("%d", 0);
    }
    FUNC_RET("%d", 1);
}

static PyObject *
Sandbox_get_task(Sandbox * self, void * closure)
{
    FUNC_BEGIN("%p,%p", self, closure);
    assert(self);
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    
    command_t * pcomm = &Sandbox_GET_SBOX(self).task.comm;
    
    size_t argc = 0;
    while ((argc + 1 < SBOX_ARG_MAX) && (pcomm->args[argc] >= 0))
    {
        argc++;
    }
    
    PyObject * tuple = PyTuple_New(argc);
    if (tuple == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    Py_ssize_t i = 0;
    for (i = 0; i < argc; i++)
    {
        PyTuple_SET_ITEM(tuple, i, 
            Py_BuildValue("s", pcomm->buff + pcomm->args[i]));
    }
    
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%p", tuple);
}

static PyObject *
Sandbox_get_quota(Sandbox * self, void * closure)
{
    FUNC_BEGIN("%p,%p", self, closure);
    assert(self);
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    
    res_t * quota = Sandbox_GET_SBOX(self).task.quota;
    
    PyObject * tuple = PyTuple_New(QUOTA_TOTAL);
    if (tuple == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    Py_ssize_t i = 0;
    for (i = 0; i < QUOTA_TOTAL; i++)
    {
        PyTuple_SET_ITEM(tuple, i, 
            Py_BuildValue("K", (unsigned long long)quota[i]));
    }
    
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%p", tuple);
}

static PyObject *
Sandbox_get_jail(Sandbox * self, void * closure)
{
    FUNC_BEGIN("%p,%p", self, closure);
    assert(self);
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    PyObject * jail = Py_BuildValue("s", Sandbox_GET_SBOX(self).task.jail);
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    if (jail == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        FUNC_RET("%p", Py_NULL);
    }
    
    FUNC_RET("%p", jail);
}

static PyObject *
Sandbox_get_policy(Sandbox * self, void * closure)
{
    FUNC_BEGIN("%p,%p", self, closure);
    assert(self);
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    PyObject * p = (PyObject *)Sandbox_GET_SBOX(self).ctrl.policy.data;
    Py_XINCREF(p);
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%p", p);
}

static int
Sandbox_set_policy(Sandbox * self, PyObject * policy, void * closure)
{
    FUNC_BEGIN("%p,%p,%p", self, policy, closure);
    assert(self);
    
    /* Should not delete policy attribute */
    if (policy == NULL)
    {
        PyErr_SetString(PyExc_AssertionError, MSG_POLICY_DEL_FORBID);
        FUNC_RET("%d", -1);
    }
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    
    /* Cannot set policy when the sandbox is running */
    if (IS_RUNNING(&Sandbox_GET_SBOX(self)) || 
        IS_BLOCKED(&Sandbox_GET_SBOX(self)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_POLICY_SET_FORBID);
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%d", -1);
    }
    
    if (!SandboxPolicy_Check(policy))
    {
        PyErr_SetString(PyExc_TypeError, MSG_POLICY_TYPE_ERR);
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%d", -1);
    }
    
    RELOCK(&Sandbox_GET_SBOX(self), EX);
    
    /* In case old == policy, del(old) may trigger garbage collection, leaving
     * policy a wild pointer, so we must incref(policy) before del(old). */
    Py_INCREF(policy);
    
    Sandbox_CLEAR_POLICY(self);
    Sandbox_GET_SBOX(self).ctrl.policy = SandboxPolicy_AS_POLICY(policy);
    
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%d", 0);
}

static PyObject *
Sandbox_get_pid(Sandbox * self, void * closure)
{
    FUNC_BEGIN("%p,%p", self, closure);
    assert(self);

    LOCK(&Sandbox_GET_SBOX(self), SH);
    PyObject * pid = NULL;
    if (IS_RUNNING(&Sandbox_GET_SBOX(self)) || \
        IS_BLOCKED(&Sandbox_GET_SBOX(self)))
    {
        pid = Py_BuildValue("i", Sandbox_GET_SBOX(self).ctrl.pid);
    }
    else
    {
        pid = Py_None;
        Py_INCREF(pid);
    }
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%p", pid);
}

static PyObject *
Sandbox_get_status(Sandbox * self, void * closure)
{
    FUNC_BEGIN("%p,%p", self, closure);
    assert(self);
    LOCK(&Sandbox_GET_SBOX(self), SH);
    PyObject * o = Py_BuildValue("i", Sandbox_GET_SBOX(self).status);
    UNLOCK(&Sandbox_GET_SBOX(self));
    FUNC_RET("%p", o);
}

static PyObject *
Sandbox_get_result(Sandbox * self, void * closure)
{
    FUNC_BEGIN("%p,%p", self, closure);
    assert(self);
    LOCK(&Sandbox_GET_SBOX(self), SH);
    PyObject * o = Py_BuildValue("i", Sandbox_GET_SBOX(self).result);
    UNLOCK(&Sandbox_GET_SBOX(self));
    FUNC_RET("%p", o);
}

static PyObject *
Sandbox_probe(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    
    /* Cannot probe a process that is not started */
    if (NOT_STARTED(&Sandbox_GET_SBOX(self)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_PROBE_NOT_STARTED);
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    /* Make a dictionary object for organizing statistics */
    PyObject * result = PyDict_New();
    if (result == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    /* Temporary variable(s) */
    PyObject * o = NULL;
    
    /* struct timespec to msec conversion */
    #ifndef ts2ms
    #define ts2ms(a) ((((a).tv_sec) * 1000) + (((a).tv_nsec) / 1000000))
    #endif
    
    PyDict_SetItemString(result, "elapsed", o = Py_BuildValue("k", \
        ts2ms(Sandbox_GET_SBOX(self).stat.elapsed)));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "cpu_info", o = Py_BuildValue("(k,k,k,K)", \
        ts2ms(Sandbox_GET_SBOX(self).stat.cpu_info.clock), \
        ts2ms(Sandbox_GET_SBOX(self).stat.cpu_info.utime), \
        ts2ms(Sandbox_GET_SBOX(self).stat.cpu_info.stime), \
        Sandbox_GET_SBOX(self).stat.cpu_info.tsc));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "mem_info", o = Py_BuildValue("(k,k,k,k,k,k)", 
        Sandbox_GET_SBOX(self).stat.mem_info.vsize / 1024,
        Sandbox_GET_SBOX(self).stat.mem_info.vsize_peak / 1024, 
        Sandbox_GET_SBOX(self).stat.mem_info.rss / 1024,
        Sandbox_GET_SBOX(self).stat.mem_info.rss_peak / 1024,
        Sandbox_GET_SBOX(self).stat.mem_info.minflt,
        Sandbox_GET_SBOX(self).stat.mem_info.majflt));
    Py_DECREF(o);
    
    union
    {
        long scno;
        syscall_t scinfo;
    } sc = {Sandbox_GET_SBOX(self).stat.syscall};
    
    PyDict_SetItemString(result, "syscall_info", 
#ifdef __x86_64__
        o = Py_BuildValue("(i,i)", sc.scinfo.scno, sc.scinfo.mode));
#else
        o = Py_BuildValue("(i,i)", sc.scno, 0));
#endif /* __x86_64__ */
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "signal_info", 
        o = Py_BuildValue("(i,i)", Sandbox_GET_SBOX(self).stat.signal.signo,
                                   Sandbox_GET_SBOX(self).stat.signal.code));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "exitcode", 
        o = Py_BuildValue("i", Sandbox_GET_SBOX(self).stat.exitcode));
    Py_DECREF(o);
    
    /* The following fields are available from cpu_info and mem_info, and are
     * no longer maintained by the probe() method of the _sandbox.Sandbox class
     * in C module. For backward compatibility, sandbox.__init__.py provides a
     * derived Sandbox class, whose probe() method can generate these fields 
     * on-the-fly (except mem.nswap). */
    
#ifdef DELETED
    PyDict_SetItemString(result, "cpu", o = Py_BuildValue("k",
        Sandbox_GET_SBOX(self).stat.ru.ru_utime.tv_sec * 1000
        + Sandbox_GET_SBOX(self).stat.ru.ru_utime.tv_usec / 1000
        + Sandbox_GET_SBOX(self).stat.ru.ru_stime.tv_sec * 1000
        + Sandbox_GET_SBOX(self).stat.ru.ru_stime.tv_usec / 1000));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "cpu.usr", o = Py_BuildValue("k",
        Sandbox_GET_SBOX(self).stat.ru.ru_utime.tv_sec * 1000
        + Sandbox_GET_SBOX(self).stat.ru.ru_utime.tv_usec / 1000));
    Py_DECREF(o);

    PyDict_SetItemString(result, "cpu.sys", o = Py_BuildValue("k",
        Sandbox_GET_SBOX(self).stat.ru.ru_stime.tv_sec * 1000
        + Sandbox_GET_SBOX(self).stat.ru.ru_stime.tv_usec / 1000));
    Py_DECREF(o);

    PyDict_SetItemString(result, "cpu.tsc", o = Py_BuildValue("K",
        Sandbox_GET_SBOX(self).stat.tsc));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "mem.vsize_peak",
        o = Py_BuildValue("k", Sandbox_GET_SBOX(self).stat.vsize_peak / 1024));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.rss_peak",
        o = Py_BuildValue("k", Sandbox_GET_SBOX(self).stat.rss_peak / 1024));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.vsize",
        o = Py_BuildValue("k", Sandbox_GET_SBOX(self).stat.vsize / 1024));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.rss",
        o = Py_BuildValue("k", Sandbox_GET_SBOX(self).stat.rss / 1024));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "mem.minflt",
        o = Py_BuildValue("k", Sandbox_GET_SBOX(self).stat.ru.ru_minflt));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.majflt",
        o = Py_BuildValue("k", Sandbox_GET_SBOX(self).stat.ru.ru_majflt));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.nswap",
        o = Py_BuildValue("k", Sandbox_GET_SBOX(self).stat.ru.ru_nswap));
    Py_DECREF(o);
#endif /* DELETED */
    
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%p", result);
}

static PyObject *
Sandbox_dump(Sandbox * self, PyObject * args)
{
    FUNC_BEGIN("%p,%p", self, args);
    assert(self && args);
    
    LOCK(&Sandbox_GET_SBOX(self), SH);
    
    /* Cannot dump a process unless it is blocked */
    if (!IS_BLOCKED(&Sandbox_GET_SBOX(self)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_DUMP_NOT_BLOCKED);
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    /* Parse input arguments */
    int type = T_LONG;          /* expected datatype */
    unsigned long addr = 0;     /* address of targeted data */
    if (!PyArg_ParseTuple(args, "ik", &type, &addr))
    {
        /* NOTE: PyArg_ParseTuple() sets exception on error */
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    /* Empty object for storing dumped data */
    PyObject * result = NULL;
    
    /* Temporary variable(s) */
    char proc[4096] = {0};      /* we don't know about proc_t internals */
    union
    {
        long data;
        short v_short;
        int v_int;
        char byte[sizeof(long)];
        double v_double;
        float v_float;
    } word = {0};
    
    /* Prototypes for calling private proc_* routines in libsandbox */
    bool proc_bind(const void * const, void * const);
    bool proc_probe(pid_t, int, void * const);
    bool proc_dump(const void * const, const void * const, size_t, char * const);
    
    proc_bind((void *)&Sandbox_GET_SBOX(self), (void *)&proc);
    
    if (!proc_probe(Sandbox_GET_SBOX(self).ctrl.pid, 0, (void *)&proc))
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_DUMP_PROBE_FAILED);
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    /* Dump data from the prisoner process */
    if (!proc_dump((void *)&proc, (void *)addr, sizeof(word), word.byte))
    {
        /* If the dump failed due to invalid address or memory out-of-range,
         * raise ValueError, otherwise raise RuntimeError */
        if ((errno == EIO) || (errno == EFAULT))
        {
            PyErr_SetString(PyExc_ValueError, MSG_DUMP_INVALID);
        }
        else
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_DUMP_FAILED);
        }
        UNLOCK(&Sandbox_GET_SBOX(self));
        FUNC_RET("%p", Py_NULL);
    }
    
    switch (type)
    {
    case T_CHAR:
    case T_BYTE:
        result = PyLong_FromLong((long)word.byte[0]);
        break;
    case T_UBYTE:
        result = PyLong_FromLong((long)(unsigned char)word.byte[0]);
        break;
    case T_SHORT:
        result = PyLong_FromLong((long)word.v_short);
        break;
    case T_USHORT:
        result = PyLong_FromLong((long)(unsigned short)word.v_short);
        break;
    case T_FLOAT:
        result = PyFloat_FromDouble((double)word.v_float);
        break;
    case T_DOUBLE:
        result = PyFloat_FromDouble(word.v_double);
        break;
    case T_INT:
        result = PyLong_FromLong((long)word.v_int);
        break;
    case T_UINT:
        result = PyLong_FromUnsignedLong((unsigned long)(unsigned int)word.v_int);
        break;
    case T_LONG:
        result = PyLong_FromLong(word.data);
        break;
    case T_ULONG:
        result = PyLong_FromUnsignedLong((unsigned long)word.data);
        break;
    case T_STRING:
    {
        result = PyBytes_FromString("");
        if (result == NULL)
        {
            if (!PyErr_Occurred())
            {
                PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
            }
            break;
        }
        bool eol = false;
        while (!eol)
        {
            size_t i = 0;
            for (i = 0; i < sizeof(word); i++, addr++)
            {
                if (word.byte[i] == '\0')
                {
                    eol = true;
                    break;
                }
            }
            PyBytes_ConcatAndDel(&result, PyBytes_FromStringAndSize(word.byte, i));
            if ((eol) || (result == NULL))
            {
                if ((result == NULL) && !PyErr_Occurred())
                {
                    PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
                }
                break;
            }
            if (!proc_dump((void *)&proc, (void *)addr, sizeof(word), word.byte))
            {
                Py_XDECREF(result);
                if ((errno == EIO) || (errno == EFAULT))
                {
                    PyErr_SetString(PyExc_ValueError, MSG_DUMP_INVALID);
                }
                else
                {
                    PyErr_SetString(PyExc_RuntimeError, MSG_DUMP_FAILED);
                }
                result = Py_NULL;
                break;
            }
        }
        break;
    }
    default:
        /* Raise TypeError in case of wrong typeid */
        PyErr_SetString(PyExc_TypeError, MSG_DUMP_TYPE_ERR);
        result = Py_NULL;
        break;
    }
    
    UNLOCK(&Sandbox_GET_SBOX(self));
    
    FUNC_RET("%p", result);
}

static PyObject *
Sandbox_run(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    
    if (!sandbox_check(&Sandbox_GET_SBOX(self)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_SBOX_CHECK_FAILED);
        FUNC_RET("%p", Py_NULL);
    }
    
    Py_BEGIN_ALLOW_THREADS
    sandbox_execute(&Sandbox_GET_SBOX(self));
    Py_END_ALLOW_THREADS
    
    Py_INCREF(Py_None);
    FUNC_RET("%p", Py_None);
}

/* sandboxModule */

static PyMethodDef moduleMethods[] = 
{
    {NULL, NULL, 0, NULL}      /* Sentinel */
};

#ifdef PY3K

typedef struct
{
    PyObject * error;
} SandboxModuleState;

static int SandboxModule_traverse(PyObject *, visitproc, void *);
static int SandboxModule_clear(PyObject *);

static struct PyModuleDef moduleDef = 
{
    PyModuleDef_HEAD_INIT,                      /* m_base */
    "_sandbox",                                 /* m_name */
    __doc__,                                    /* m_doc */
    sizeof(SandboxModuleState),                 /* m_size */
    moduleMethods,                              /* m_methods */
    NULL,                                       /* m_reload */
    SandboxModule_traverse,                     /* m_traverse */
    SandboxModule_clear,                        /* m_clear */
    NULL,                                       /* m_free */
};

#define MODULE_STATE(m) ((SandboxModuleState *)PyModule_GetState(m))

static int 
SandboxModule_traverse(PyObject * m, visitproc visit, void * arg) 
{
    FUNC_BEGIN("%p,%p,%p", m, visit, arg);
    Py_VISIT(MODULE_STATE(m)->error);
    FUNC_RET("%d", 0);
}

static int 
SandboxModule_clear(PyObject * m) 
{
    FUNC_BEGIN("%p", m);
    Py_CLEAR(MODULE_STATE(m)->error);
    FUNC_RET("%d", 0);
}

#define MODULE_INIT(x) PyObject * PyInit_ ## x (void)
#define INIT_BEGIN FUNC_BEGIN
#define INIT_RET(m) FUNC_RET("%p", m)

#else /* Python 2 */

#define MODULE_INIT(x) void init ## x (void)
#define INIT_BEGIN PROC_BEGIN
#define INIT_RET(m) PROC_END()

#endif /* PY3K */

MODULE_INIT(_sandbox)
{
    INIT_BEGIN();
    
    PyObject * o = NULL;
    
    /* Initialize the sandbox module */
#ifdef PY3K
    PyObject * module = PyModule_Create(&moduleDef);
#else /* Python 2 */
    PyObject * module = Py_InitModule3("_sandbox", moduleMethods, __doc__);
#endif /* PY3K */
    
    if (module == NULL)
    {
        INIT_RET(Py_NULL);
    }
    
#ifdef PY3K
    SandboxModuleState *st = MODULE_STATE(module);
    st->error = PyErr_NewException("_sandbox.Error", NULL, NULL);
    if (st->error == NULL)
    {
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
#endif /* PY3K */
    
    /* Package version */
    if (PyObject_SetAttrString(module, "__version__", 
        o = Py_BuildValue("s", VERSION)) != 0)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ATTR_ADD_FAILED);
        }
        Py_DECREF(o);
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    Py_DECREF(o);
    
    if (PyObject_SetAttrString(module, "__author__",
        o = Py_BuildValue("s", AUTHOR)) != 0)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ATTR_ADD_FAILED);
        }
        Py_DECREF(o);
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    Py_DECREF(o);
    
    /* Finalize the generic any type */
    anyType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&anyType) != 0)
    {
        /* NOTE: PyType_Ready() sets exception on error */
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Add anyType to sandbox module */
    Py_INCREF(&anyType);
    if (PyModule_AddObject(module, "Any", 
        (PyObject *)&anyType) != 0)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        }
        Py_DECREF(&anyType);
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    DBUG("added anyType to module");
    
    /* Prepare constant attributes for eventType */
    eventType.tp_dict = PyDict_New();
    if (eventType.tp_dict == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Wrapper items for constants in event_type_t */
    PyDict_SetItemString(eventType.tp_dict, "S_EVENT_ERROR", 
        o = Py_BuildValue("i", S_EVENT_ERROR));
    Py_DECREF(o);
    PyDict_SetItemString(eventType.tp_dict, "S_EVENT_EXIT", 
        o = Py_BuildValue("i", S_EVENT_EXIT));
    Py_DECREF(o);
    PyDict_SetItemString(eventType.tp_dict, "S_EVENT_SIGNAL", 
        o = Py_BuildValue("i", S_EVENT_SIGNAL));
    Py_DECREF(o);
    PyDict_SetItemString(eventType.tp_dict, "S_EVENT_SYSCALL", 
        o = Py_BuildValue("i", S_EVENT_SYSCALL));
    Py_DECREF(o);
    PyDict_SetItemString(eventType.tp_dict, "S_EVENT_SYSRET", 
        o = Py_BuildValue("i", S_EVENT_SYSRET));
    Py_DECREF(o);
    PyDict_SetItemString(eventType.tp_dict, "S_EVENT_QUOTA", 
        o = Py_BuildValue("i", S_EVENT_QUOTA));
    Py_DECREF(o);
    
    /* Finalize the sandbox event type */
    eventType.tp_base = &anyType;
    if (PyType_Ready(&eventType) != 0)
    {
        /* NOTE: PyType_Ready() sets exception on error */
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Add eventType to sandbox module */
    Py_INCREF(&eventType);
    if (PyModule_AddObject(module, "SandboxEvent", 
        (PyObject *)&eventType) != 0)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        }
        Py_DECREF(&eventType);
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    DBUG("added eventType to module");
    
    /* Prepare constant attributes for actionType */
    actionType.tp_dict = PyDict_New();
    if (actionType.tp_dict == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Wrapper items for constants in action_type_t */
    PyDict_SetItemString(actionType.tp_dict, "S_ACTION_CONT", 
        o = Py_BuildValue("i", S_ACTION_CONT));
    Py_DECREF(o);
    PyDict_SetItemString(actionType.tp_dict, "S_ACTION_FINI", 
        o = Py_BuildValue("i", S_ACTION_FINI));
    Py_DECREF(o);
    PyDict_SetItemString(actionType.tp_dict, "S_ACTION_KILL", 
        o = Py_BuildValue("i", S_ACTION_KILL));
    Py_DECREF(o);
    
    /* Finalize the sandbox action type */
    actionType.tp_base = &anyType;
    if (PyType_Ready(&actionType) != 0)
    {
        /* NOTE: PyType_Ready() sets exception on error */
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Add actionType to sandbox module */
    Py_INCREF(&actionType);
    if (PyModule_AddObject(module, "SandboxAction", 
        (PyObject *)&actionType) != 0)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        }
        Py_DECREF(&actionType);
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    DBUG("added actionType to module");
    
    /* Finalize the sandbox policy type */
    policyType.tp_base = &anyType;
    if (PyType_Ready(&policyType) != 0)
    {
        /* NOTE: PyType_Ready() sets exception on error */
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Add policyType to sandbox module */
    Py_INCREF(&policyType);
    if (PyModule_AddObject(module, "SandboxPolicy", 
        (PyObject *)&policyType) != 0)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        }
        Py_DECREF(&policyType);
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    DBUG("added policyType to module");
    
    /* Prepare constant attributes for sandboxType */
    sandboxType.tp_dict = PyDict_New();
    if (sandboxType.tp_dict == NULL)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Wrapper items for constants in quota_type_t */
    PyDict_SetItemString(sandboxType.tp_dict, "S_QUOTA_WALLCLOCK", 
        o = Py_BuildValue("i", S_QUOTA_WALLCLOCK));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_QUOTA_CPU", 
        o = Py_BuildValue("i", S_QUOTA_CPU));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_QUOTA_MEMORY", 
        o = Py_BuildValue("i", S_QUOTA_MEMORY));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_QUOTA_DISK", 
        o = Py_BuildValue("i", S_QUOTA_DISK));
    Py_DECREF(o);
    
    /* Wrapper items for constants in status_t */
    PyDict_SetItemString(sandboxType.tp_dict, "S_STATUS_PRE", 
        o = Py_BuildValue("i", S_STATUS_PRE));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_STATUS_RDY", 
        o = Py_BuildValue("i", S_STATUS_RDY));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_STATUS_EXE", 
        o = Py_BuildValue("i", S_STATUS_EXE));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_STATUS_BLK", 
        o = Py_BuildValue("i", S_STATUS_BLK));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_STATUS_FIN", 
        o = Py_BuildValue("i", S_STATUS_FIN));
    Py_DECREF(o);
    
    /* Wrapper items for constants in result_t */
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_PD", 
        o = Py_BuildValue("i", S_RESULT_PD));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_OK", 
        o = Py_BuildValue("i", S_RESULT_OK));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_RF", 
        o = Py_BuildValue("i", S_RESULT_RF));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_ML", 
        o = Py_BuildValue("i", S_RESULT_ML));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_OL", 
        o = Py_BuildValue("i", S_RESULT_OL));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_TL", 
        o = Py_BuildValue("i", S_RESULT_TL));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_RT", 
        o = Py_BuildValue("i", S_RESULT_RT));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_AT", 
        o = Py_BuildValue("i", S_RESULT_AT));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_IE", 
        o = Py_BuildValue("i", S_RESULT_IE));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_BP",
        o = Py_BuildValue("i", S_RESULT_BP));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_R0",
        o = Py_BuildValue("i", S_RESULT_R0));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_R1",
        o = Py_BuildValue("i", S_RESULT_R1));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_R2",
        o = Py_BuildValue("i", S_RESULT_R2));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_R3",
        o = Py_BuildValue("i", S_RESULT_R3));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_R4",
        o = Py_BuildValue("i", S_RESULT_R4));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "S_RESULT_R5",
        o = Py_BuildValue("i", S_RESULT_R5));
    Py_DECREF(o);

    /* Wrapper items for constants in structmembers.h */
    PyDict_SetItemString(sandboxType.tp_dict, "T_BYTE",
        o = Py_BuildValue("i", T_BYTE));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_SHORT",
        o = Py_BuildValue("i", T_SHORT));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_INT",
        o = Py_BuildValue("i", T_INT));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_LONG",
        o = Py_BuildValue("i", T_LONG));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_UBYTE",
        o = Py_BuildValue("i", T_UBYTE));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_USHORT",
        o = Py_BuildValue("i", T_USHORT));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_UINT",
        o = Py_BuildValue("i", T_UINT));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_ULONG",
        o = Py_BuildValue("i", T_ULONG));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_FLOAT",
        o = Py_BuildValue("i", T_FLOAT));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_DOUBLE",
        o = Py_BuildValue("i", T_DOUBLE));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_STRING",
        o = Py_BuildValue("i", T_STRING));
    Py_DECREF(o);
    PyDict_SetItemString(sandboxType.tp_dict, "T_CHAR",
        o = Py_BuildValue("i", T_CHAR));
    Py_DECREF(o);

    /* Finalize the sandbox policy type */
    sandboxType.tp_base = &anyType;
    if (PyType_Ready(&sandboxType) != 0)
    {
        /* NOTE: PyType_Ready() sets exception on error */
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    
    /* Add sandboxType to sandbox module */
    Py_INCREF(&sandboxType);
    if (PyModule_AddObject(module, "Sandbox", (PyObject *)&sandboxType) != 0)
    {
        if (!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        }
        Py_DECREF(&sandboxType);
        Py_DECREF(module);
        INIT_RET(Py_NULL);
    }
    DBUG("added sandboxType to module");
    
    /* Acquire GIL */
    PyEval_InitThreads();
    
    INIT_RET(module);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
