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

#include "module.h"
#include <bytesobject.h>       /* py3k forward compatibility */
#include <sandbox-dev.h>       /* libsandbox internals */
#include <pwd.h>               /* struct passwd, getpwnam(), getpwuid() */
#include <grp.h>               /* struct group, getgrnam(), getgrgid() */

#ifdef __cplusplus
extern "C"
{
#endif

/* SandboxEventType */
static PyObject * SandboxEvent_alloc(PyTypeObject *, PyObject *, PyObject *);
static int SandboxEvent_init(SandboxEvent *, PyObject *, PyObject *);
static void SandboxEvent_free(SandboxEvent *);

static PyMemberDef SandboxEventMembers[] = 
{
    {"type", T_INT, offsetof(SandboxEvent, raw.type), RESTRICTED, NULL},
    {"data", T_INT, offsetof(SandboxEvent, raw.data), RESTRICTED, NULL},
/* In 32bit systems, the width of each native field in raw.data is 32 bits, and 
 * both 'data' and 'ext0' interpret the first 32 bits at the beginning of 
 * raw.data as a 32bit int; in 64bit systems, 'data' still represents the first 
 * 32bit int, whereas 'ext0' represents the second 32bit int of raw.data */
#ifdef __x86_64__
    {"ext0", T_INT, offsetof(SandboxEvent, raw.data.__bitmap__.A) + 4, 
     RESTRICTED, NULL},
#else
    {"ext0", T_INT, offsetof(SandboxEvent, raw.data.__bitmap__.A), 
     RESTRICTED, NULL},
#endif /* __x86_64__ */
    {"ext1", T_LONG, offsetof(SandboxEvent, raw.data.__bitmap__.B), RESTRICTED, 
     NULL},
    {"ext2", T_LONG, offsetof(SandboxEvent, raw.data.__bitmap__.C), RESTRICTED, 
     NULL},
    {"ext3", T_LONG, offsetof(SandboxEvent, raw.data.__bitmap__.D), RESTRICTED, 
     NULL},
    {"ext4", T_LONG, offsetof(SandboxEvent, raw.data.__bitmap__.E), RESTRICTED, 
     NULL},
    {"ext5", T_LONG, offsetof(SandboxEvent, raw.data.__bitmap__.F), RESTRICTED, 
     NULL},
    {"ext6", T_LONG, offsetof(SandboxEvent, raw.data.__bitmap__.G), RESTRICTED, 
     NULL},
    {NULL, 0, 0, 0, NULL}       /* Sentinel */
};

static PyTypeObject SandboxEventType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "SandboxEvent",                           /* tp_name */
    sizeof(SandboxEvent),                     /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)SandboxEvent_free,            /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                       /* tp_flags */
    DOC_TP_EVENT,                             /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    SandboxEventMembers,                      /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)SandboxEvent_init,              /* tp_init */
    0,                                        /* tp_alloc */
    SandboxEvent_alloc,                       /* tp_new */
};

static PyObject *
SandboxEvent_alloc(PyTypeObject * type, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", type, args, keys);
    assert(type && args);
    SandboxEvent * self = (SandboxEvent *)type->tp_alloc(type, 0);
    FUNC_RET("%p", (PyObject *)self);
}

static int 
SandboxEvent_init(SandboxEvent * self, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", self, args, keys);
    assert(self && args);
    if (!PyArg_ParseTuple(args, "i|lllllll",
        &self->raw.type,
        &self->raw.data.__bitmap__.A,
        &self->raw.data.__bitmap__.B,
        &self->raw.data.__bitmap__.C,
        &self->raw.data.__bitmap__.D,
        &self->raw.data.__bitmap__.E,
        &self->raw.data.__bitmap__.F,
        &self->raw.data.__bitmap__.G))
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
    FUNC_RET("%d", PyObject_TypeCheck(o, &SandboxEventType));
}

static event_t *
SandboxEvent_AS_EVENT(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    FUNC_RET("%p", &((SandboxEvent *)o)->raw);
}

static int 
SandboxEvent_SET_EVENT(PyObject * o, const event_t * event)
{
    FUNC_BEGIN("%p,%p", o, event);
    if (event != NULL)
    {
        memcpy(&((SandboxEvent *)o)->raw, event, sizeof(event_t));
    }
    else
    {
        memset(&((SandboxEvent *)o)->raw, 0, sizeof(event_t));
    }
    FUNC_RET("%d", 0);
}

static int
SandboxEvent_SetEvent(PyObject * o, const event_t * event)
{
    FUNC_BEGIN("%p,%p", o, event);
    if (!SandboxEvent_Check(o))
    {
        FUNC_RET("%d", -1);
    }
    FUNC_RET("%d", SandboxEvent_SET_EVENT(o, event));
}

static PyObject *
SandboxEvent_NewEvent(void)
{
    FUNC_BEGIN();
    PyObject * pEvent = PyType_GenericAlloc(&SandboxEventType, 1);
    if (pEvent == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        FUNC_RET("%p", NULL);
    }
    FUNC_RET("%p", pEvent);
}

static PyObject *
SandboxEvent_FromEvent(const event_t * event)
{
    FUNC_BEGIN("%p", event);
    PyObject * pEvent = SandboxEvent_NewEvent();
    SandboxEvent_SetEvent(pEvent, event);
    FUNC_RET("%p", pEvent);
}

/* SandboxActionType */
static PyObject * SandboxAction_alloc(PyTypeObject *, PyObject *, PyObject *);
static int SandboxAction_init(SandboxAction *, PyObject *, PyObject *);
static void SandboxAction_free(SandboxAction *);

static PyMemberDef SandboxActionMembers[] = 
{
    {"type", T_INT, offsetof(SandboxAction, raw.type), RESTRICTED, NULL},
    {"data", T_INT, offsetof(SandboxAction, raw.data), RESTRICTED, NULL},
    {NULL, 0, 0, 0, NULL}       /* Sentinel */
};

static PyTypeObject SandboxActionType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "SandboxAction",                          /* tp_name */
    sizeof(SandboxAction),                    /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)SandboxAction_free,           /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                       /* tp_flags */
    DOC_TP_ACTION,                            /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    SandboxActionMembers,                     /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)SandboxAction_init,             /* tp_init */
    0,                                        /* tp_alloc */
    SandboxAction_alloc,                      /* tp_new */
};

static PyObject *
SandboxAction_alloc(PyTypeObject * type, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", type, args, keys);
    assert(type && args);
    SandboxAction * self = (SandboxAction *)type->tp_alloc(type, 0);
    FUNC_RET("%p", (PyObject *)self);
}

static int 
SandboxAction_init(SandboxAction * self, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", self, args, keys);
    assert(self && args);
    if (!PyArg_ParseTuple(args, "i|ll", &self->raw.type,
                                        &self->raw.data.__bitmap__.A,
                                        &self->raw.data.__bitmap__.B))
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
    FUNC_RET("%d", PyObject_TypeCheck(o, &SandboxActionType));
}

static action_t *
SandboxAction_AS_ACTION(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    FUNC_RET("%p", &((SandboxAction *)o)->raw);
}

static int
SandboxAction_SET_ACTION(PyObject * o, const action_t * action)
{
    FUNC_BEGIN("%p,%p", o, action);
    if (action != NULL)
    {
        memcpy(&((SandboxAction *)o)->raw, action, sizeof(action_t));
    }
    else
    {
        memset(&((SandboxAction *)o)->raw, 0, sizeof(action_t));
    }
    FUNC_RET("%d", 0);
}

static int 
SandboxAction_SetAction(PyObject * o, const action_t * action)
{
    FUNC_BEGIN("%p,%p", o, action);
    if (!SandboxAction_Check(o))
    {
        FUNC_RET("%d", -1);
    }
    FUNC_RET("%d", SandboxAction_SET_ACTION(o, action));
}

static PyObject *
SandboxAction_NewAction(void)
{
    FUNC_BEGIN();
    PyObject * pAction = PyType_GenericAlloc(&SandboxActionType, 1);
    if (pAction == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        FUNC_RET("%p", NULL);
    }
    FUNC_RET("%p", pAction);
}

static PyObject *
SandboxAction_FromAction(const action_t * action)
{
    FUNC_BEGIN("%p", action);
    PyObject * pAction = SandboxAction_NewAction();
    SandboxAction_SetAction(pAction, action);
    FUNC_RET("%p", pAction);
}

/* SandboxPolicyType */
static PyObject * SandboxPolicy_alloc(PyTypeObject *, PyObject *, PyObject *);
static int SandboxPolicy_init(SandboxPolicy *, PyObject *, PyObject *);
static PyObject * SandboxPolicy_call(SandboxPolicy *, PyObject *, PyObject *);
static void SandboxPolicy_free(SandboxPolicy *);

static PyTypeObject SandboxPolicyType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "SandboxPolicy",                          /* tp_name */
    sizeof(SandboxPolicy),                    /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)SandboxPolicy_free,           /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    (ternaryfunc)SandboxPolicy_call,          /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    DOC_TP_POLICY,                            /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)SandboxPolicy_init,             /* tp_init */
    0,                                        /* tp_alloc */
    SandboxPolicy_alloc,                      /* tp_new */
};

static PyObject *
SandboxPolicy_alloc(PyTypeObject * type, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", type, args, keys);
    assert(type && args);
    SandboxPolicy * self = (SandboxPolicy *)type->tp_alloc(type, 0);
    FUNC_RET("%p", (PyObject *)self);
}

static int 
SandboxPolicy_init(SandboxPolicy * self, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", self, args, keys);
    assert(self && args);
    FUNC_RET("%d", 0);
}

static void
SandboxPolicy_free(SandboxPolicy * self)
{
    PROC_BEGIN("%p", self);
    assert(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
    PROC_END();
}

#ifdef DELETED
static void SandboxPolicy_default_policy(const event_t *, action_t *);
#endif /* DELETED */

static PyObject *
SandboxPolicy_call(SandboxPolicy * self, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", self, args, keys);
    assert(self && args);

    PyObject * pEvent = NULL;
    PyObject * pAction = NULL;

    if (!PyArg_ParseTuple(args, "OO", &pEvent, &pAction))
    {
        FUNC_RET("%p", NULL);
    }
    
    if (!SandboxEvent_Check(pEvent) || !SandboxAction_Check(pAction))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_POLICY_CALL_FAILED);
        FUNC_RET("%p", NULL);
    }
    
    /* Since 0.3.3-rc4, the core library exports a baseline policy function
     * sandbox_default_policy() for preliminary (minimal black list) sandboxing.
     * The calling programs of the core library, i.e. pysandbox, no longer need 
     * to include local baseline policies. They should, of course, override the 
     * baseline policy if more sophisticated sandboxing rules are desired. */
    
#ifdef DELETED
    SandboxPolicy_default_policy
#else
    sandbox_default_policy
#endif /* DELETED */
    (
        NULL,
        SandboxEvent_AS_EVENT(pEvent), 
        SandboxAction_AS_ACTION(pAction)
    );
    
    FUNC_RET("%p", pAction);
}

static int
SandboxPolicy_Check(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    if (o == NULL)
    {
        FUNC_RET("%d", 0);
    }
    FUNC_RET("%d", PyObject_TypeCheck(o, &SandboxPolicyType));
}

static PyObject *
SandboxPolicy_New(void)
{
    FUNC_BEGIN();
    PyObject * args = Py_BuildValue("()");
    PyObject * o = PyType_GenericNew(&SandboxPolicyType, args, NULL);
    Py_DECREF(args);
    FUNC_RET("%p", o);
}

#ifdef DELETED
static void 
SandboxPolicy_default_policy(const event_t * pevent, action_t * paction)
{
    PROC_BEGIN("%p,%p", pevent, paction);
    assert(pevent && paction);
    switch (pevent->type)
    {
    case S_EVENT_SYSCALL:
    case S_EVENT_SYSRET:
        *paction = (action_t){S_ACTION_CONT};
        break;
    case S_EVENT_EXIT:
        switch (pevent->data._EXIT.code)
        {
        case EXIT_SUCCESS:
            *paction = (action_t){S_ACTION_FINI, {{S_RESULT_OK}}};
            break;
        default:
            *paction = (action_t){S_ACTION_FINI, {{S_RESULT_AT}}};
            break;
        }
        break;
    case S_EVENT_ERROR:
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_IE}}};
        break;
    case S_EVENT_SIGNAL:
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_RT}}};
        break;
    case S_EVENT_QUOTA:
        switch (pevent->data._QUOTA.type)
        {
        case S_QUOTA_WALLCLOCK:
        case S_QUOTA_CPU:
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_TL}}};
            break;
        case S_QUOTA_MEMORY:
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_ML}}};
            break;
        case S_QUOTA_DISK:
            *paction = (action_t){S_ACTION_KILL, {{S_RESULT_OL}}};
            break;
        }
        break;
    default:
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_IE}}};
        break;
    }
    PROC_END();
}
#endif /* DELETED */

static PyObject * Sandbox_run(Sandbox *);
static PyObject * Sandbox_probe(Sandbox *);
static PyObject * Sandbox_dump(Sandbox *, PyObject *);

#ifdef DELETED
static PyObject * Sandbox_start(Sandbox *);
static PyObject * Sandbox_wait(Sandbox *);
static PyObject * Sandbox_stop(Sandbox *);
#endif /* DELETED */

static PyMethodDef SandboxMethods[] = 
{
    {"dump", (PyCFunction)Sandbox_dump, METH_VARARGS, DOC_SANDBOX_DUMP},
    {"probe", (PyCFunction)Sandbox_probe, METH_NOARGS, DOC_SANDBOX_PROBE},
    {"run", (PyCFunction)Sandbox_run, METH_NOARGS, DOC_SANDBOX_RUN},
#ifdef DELETED
    {"start", (PyCFunction)Sandbox_start, METH_NOARGS,
     "Start to run the task if it is ready and return immediately"},
    {"wait", (PyCFunction)Sandbox_wait, METH_NOARGS,
     "Wait for the started task to complete"},
    {"stop", (PyCFunction)Sandbox_stop, METH_NOARGS,
     "Force the sandbox to stop its running task"},
#endif /* DELETED */
    {NULL, NULL, 0, NULL}       /* Sentinel */
};

static PyMemberDef SandboxMembers[] = 
{
    {"status", T_INT, offsetof(Sandbox, sbox.status), READONLY, 
     DOC_SANDBOX_STATUS},
    {"result", T_INT, offsetof(Sandbox, sbox.result), READONLY, 
     DOC_SANDBOX_RESULT},
    {NULL, 0, 0, 0, NULL}       /* Sentinel */
};

static PyObject * Sandbox_alloc(PyTypeObject *, PyObject *, PyObject *);
static int Sandbox_init(Sandbox *, PyObject *, PyObject *);
static void Sandbox_free(Sandbox *);

static PyTypeObject SandboxType = 
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "Sandbox",                                /* tp_name */
    sizeof(Sandbox),                          /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)Sandbox_free,                 /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    DOC_TP_SANDBOX,                           /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    SandboxMethods,                           /* tp_methods */
    SandboxMembers,                           /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)Sandbox_init,                   /* tp_init */
    0,                                        /* tp_alloc */
    Sandbox_alloc,                            /* tp_new */
};

static PyObject *
Sandbox_alloc(PyTypeObject * type, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", type, args, keys);
    assert(type && args);
    Sandbox * self = (Sandbox *)type->tp_alloc(type, 0);
    if (sandbox_init(&self->sbox, NULL) != 0)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        FUNC_RET("%p", NULL);
    }
    FUNC_RET("%p", (PyObject *)self);
}

static void
Sandbox_free(Sandbox * self)
{
    PROC_BEGIN("%p", self);
    assert(self);
    Py_XDECREF((PyObject *)self->sbox.ctrl.policy.data);
    sandbox_fini(&self->sbox);
    Py_TYPE(self)->tp_free((PyObject*)self);
    PROC_END();
}

static void Sandbox_policy_entry(const policy_t *, const event_t *, action_t *);
static int Sandbox_init_comm(PyObject *, command_t *);
static int Sandbox_init_jail(PyObject *, char *);
static int Sandbox_init_uid(PyObject *, uid_t *);
static int Sandbox_init_gid(PyObject *, gid_t *);
static int Sandbox_init_ifd(PyObject *, int *);
static int Sandbox_init_ofd(PyObject *, int *);
static int Sandbox_init_efd(PyObject *, int *);
static int Sandbox_init_qta(PyObject *, res_t *);
static int Sandbox_init_policy(PyObject *, PyObject * *);

static int
Integer_Check(PyObject * o)
{
    FUNC_BEGIN("%p", o);
    assert(o);
    
    if (PyLong_Check(o))
    {
        FUNC_RET("%d", 1);
    }
#ifdef IS_PY3K
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
    FUNC_RET("%p", NULL);
}

static int 
Sandbox_init(Sandbox * self, PyObject * args, PyObject * keys)
{
    FUNC_BEGIN("%p,%p,%p", self, args, keys);
    assert(self && args);
    
    self->sbox.ctrl.policy.entry = (void *)Sandbox_policy_entry;
    self->sbox.ctrl.policy.data = (long)SandboxPolicy_New();
    
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
    
    if (!PyArg_ParseTupleAndKeywords(args, keys, 
        "O&|O&O&O&O&O&O&O&O&", keywords, 
        Sandbox_init_comm, &self->sbox.task.comm, 
        Sandbox_init_jail, &self->sbox.task.jail, 
        Sandbox_init_uid, &self->sbox.task.uid, 
        Sandbox_init_gid, &self->sbox.task.gid, 
        Sandbox_init_ifd, &self->sbox.task.ifd, 
        Sandbox_init_ofd, &self->sbox.task.ofd, 
        Sandbox_init_efd, &self->sbox.task.efd, 
        Sandbox_init_qta, &self->sbox.task.quota,
        Sandbox_init_policy, &self->sbox.ctrl.policy.data))
    {
        FUNC_RET("%d", -1);
    }
    
    FUNC_RET("%d", 0);
}

static void 
Sandbox_policy_entry(const policy_t * ppolicy, const event_t * pevent, 
                     action_t * paction)
{
    PROC_BEGIN("%p,%p,%p", ppolicy, pevent, paction);
    
    assert(ppolicy && pevent && paction);
    
    PyObject * pPolicy = (PyObject *)ppolicy->data;
    
    PyObject * pEvent = SandboxEvent_FromEvent(pevent);
    PyObject * pAction = SandboxAction_FromAction(paction);
    
    PyObject * oAction = PyObject_CallFunction(pPolicy, "OO", pEvent, pAction);
    
    if ((PyErr_Occurred() != NULL) || (oAction == NULL) || 
        (oAction == Py_None) || !SandboxAction_Check(oAction))
    {
        Py_XDECREF(oAction);
        *paction = (action_t){S_ACTION_KILL, {{S_RESULT_BP}}};
    }
    else
    {
        if (PyObject_RichCompareBool(oAction, pAction, Py_NE))
        {
            Py_XDECREF(pAction);
            pAction = oAction;
        }
        *paction = *SandboxAction_AS_ACTION(pAction);
    }
    
    Py_XDECREF(pEvent);
    Py_XDECREF(pAction);
    
    PROC_END();
}

static int
Sandbox_init_comm(PyObject * o, command_t * pcmd)
{
    FUNC_BEGIN("%p,%p", o, pcmd);
    assert(o && pcmd);
    
    command_t target;
    
    memset(&target, 0, sizeof(command_t));
    
    size_t argc = 0;
    size_t offset = 0;
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        o = UTF8Bytes_FromObject(o);
        if (o == NULL)
        {
            FUNC_RET("%d", 0);
        }
        size_t delta = PyBytes_GET_SIZE(o) + 1;
        if (offset + delta < sizeof(target.buff))
        {
            strcpy(target.buff, PyBytes_AS_STRING(o));
            target.args[argc++] = offset;
            offset += delta;
            Py_DECREF(o);
        }
        else
        {
            Py_DECREF(o);
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
    
    if (access(target.buff, X_OK | F_OK) < 0)
    {
        PyErr_SetString(PyExc_ValueError, MSG_ARGS_INVALID);
        FUNC_RET("%d", 0);
    }
    
    struct stat s;
    if ((stat(target.buff, &s) < 0) || !S_ISREG(s.st_mode))
    {
        PyErr_SetString(PyExc_ValueError, MSG_ARGS_INVALID);
        FUNC_RET("%d", 0);
    }
    
    memcpy(pcmd, &target, sizeof(command_t));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_init_jail(PyObject * o, char * jail)
{
    FUNC_BEGIN("%p,%p", o, jail);
    assert(o && jail);
    
    char target[SBOX_PATH_MAX] = {'/', '\0'};
    
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        o = UTF8Bytes_FromObject(o);
        if (o == NULL)
        {
            FUNC_RET("%d", 0);
        }
        if (PyBytes_GET_SIZE(o) + 1 < sizeof(target))
        {
            strcpy(target, PyBytes_AS_STRING(o));
            Py_DECREF(o);
        }
        else
        {
            Py_DECREF(o);
            PyErr_SetString(PyExc_OverflowError, MSG_JAIL_TOO_LONG);
            FUNC_RET("%d", 0);
        }
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_JAIL_TYPE_ERR);
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
    
    /* no one but the super-user can chroot */
    if ((getuid() != (uid_t)0) && (strcmp(target, "/") != 0))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_JAIL_NOPERM);
        FUNC_RET("%d", 0);
    }
    
    memcpy(jail, target, sizeof(target));
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_init_uid(PyObject * o, uid_t * puid)
{
    FUNC_BEGIN("%p,%p", o, puid);
    assert(o && puid);

    uid_t uid = getuid();

    struct passwd * pw = NULL;
    
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        o = UTF8Bytes_FromObject(o);
        if (o == NULL)
        {
            FUNC_RET("%d", 0);
        }
        if ((pw = getpwnam(PyBytes_AS_STRING(o))) != NULL)
        {
            uid = pw->pw_uid;
            Py_DECREF(o);
        }
        else
        {
            Py_DECREF(o);
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
    
    /* no one but the super-user can setuid */
    if ((getuid() != (uid_t)0) && (getuid() != uid))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_OWNER_NOPERM);
        FUNC_RET("%d", 0);
    }
    
    *puid = uid;
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_init_gid(PyObject * o, gid_t * pgid)
{
    FUNC_BEGIN("%p,%p", o, pgid);
    assert(o && pgid);

    gid_t gid = getgid();
    
    struct group * gr = NULL;
    
    if (PyBytes_Check(o) || PyUnicode_Check(o))
    {
        o = UTF8Bytes_FromObject(o);
        if (o == NULL)
        {
            FUNC_RET("%d", 0);
        }
        if ((gr = getgrnam(PyBytes_AS_STRING(o))) != NULL)
        {
            gid = gr->gr_gid;
            Py_DECREF(o);
        }
        else
        {
            Py_DECREF(o);
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
    
    /* no one but the super-user can setgid */
    if ((getuid() != (uid_t)0) && (getgid() != gid))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_GROUP_NOPERM);
        FUNC_RET("%d", 0);
    }
    
    *pgid = gid;
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_init_ifd(PyObject * o, int * pifd)
{
    FUNC_BEGIN("%p,%p", o, pifd);
    
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
    if ((fstat(ifd, &s) < 0) || !(S_ISCHR(s.st_mode) || S_ISREG(s.st_mode) || 
         S_ISFIFO(s.st_mode)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDIN_INVALID);
        FUNC_RET("%d", 0);
    }
    
    if (!(((S_IRUSR & s.st_mode) && (s.st_uid == getuid())) || 
          ((S_IRGRP & s.st_mode) && (s.st_gid == getgid())) || 
          (S_IROTH & s.st_mode) || (getuid() == (uid_t)0)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDIN_INVALID);
        FUNC_RET("%d", 0);
    }
    
    *pifd = ifd;
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_init_ofd(PyObject * o, int * pofd)
{
    FUNC_BEGIN("%p,%p", o, pofd);
    
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
    if ((fstat(ofd, &s) < 0) || !(S_ISCHR(s.st_mode) || S_ISREG(s.st_mode) || 
         S_ISFIFO(s.st_mode)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDOUT_INVALID);
        FUNC_RET("%d", 0);
    }
    
    if (!(((S_IWUSR & s.st_mode) && (s.st_uid == getuid())) || 
          ((S_IWGRP & s.st_mode) && (s.st_gid == getgid())) || 
          (S_IWOTH & s.st_mode) || (getuid() == (uid_t)0)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDOUT_INVALID);
        FUNC_RET("%d", 0);
    }
    
    *pofd = ofd;
    
    FUNC_RET("%d", 1);
}

static int
Sandbox_init_efd(PyObject * o, int * pefd)
{
    FUNC_BEGIN("%p,%p", o, pefd);
    
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
    if ((fstat(efd, &s) < 0) || !(S_ISCHR(s.st_mode) || S_ISREG(s.st_mode) || 
         S_ISFIFO(s.st_mode)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDERR_INVALID);
        FUNC_RET("%d", 0);
    }
    
    if (!(((S_IWUSR & s.st_mode) && (s.st_uid == getuid())) || 
          ((S_IWGRP & s.st_mode) && (s.st_gid == getgid())) || 
          (S_IWOTH & s.st_mode) || (getuid() == (uid_t)0)))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_STDERR_INVALID);
        FUNC_RET("%d", 0);
    }
    
    *pefd = efd;
    
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
Sandbox_init_qta(PyObject * o, res_t * pqta)
{
    FUNC_BEGIN("%p,%p", o, pqta);
    
    res_t quota[QUOTA_TOTAL];
    
    memset(quota, 0, QUOTA_TOTAL * sizeof(res_t));
    
    const char * keys[] = 
    {
        "wallclock",
        "cpu",
        "memory",
        "disk",
        NULL                    /* Sentinel */
    };
    
    int t = 0;
    
    if (PyDict_Check(o))
    {
        for (t = 0; (t < QUOTA_TOTAL) && !PyErr_Occurred(); t++)
        {
            PyObject * value = NULL;
            if ((value = PyDict_GetItemString(o, (char *)keys[t])) == NULL)
            {
                quota[t] = RES_INFINITY;
                continue;
            }
            quota[t] = SandboxQuota_FromObject(value);
        }
    }
    else if (PySequence_Check(o))
    {
        for (t = 0; (t < QUOTA_TOTAL) && !PyErr_Occurred(); t++)
        {
            if (t > PySequence_Size(o))
            {
                quota[t] = RES_INFINITY;
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
    
    memcpy(pqta, quota, QUOTA_TOTAL * sizeof(res_t));
    FUNC_RET("%d", 1);
}

static int
Sandbox_init_policy(PyObject * o, PyObject * * pppolicy)
{
    FUNC_BEGIN("%p,%p", o, *pppolicy);
    
    PyObject * callback = NULL;
    
    if (SandboxPolicy_Check(o))
    {
        callback = o;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, MSG_POLICY_TYPE_ERR);
        FUNC_RET("%d", 0);
    }
    
    Py_XINCREF(callback);
    Py_XDECREF((PyObject *)*pppolicy);
    *pppolicy = callback;
    
    FUNC_RET("%d", 1);
}

static PyObject *
Sandbox_probe(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    
    P(&self->sbox.mutex);
    
    /* Cannot probe a process that is not started */
    if (NOT_STARTED(&self->sbox))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_PROBE_NOT_STARTED);
        V(&self->sbox.mutex);
        FUNC_RET("%p", NULL);
    }
    
#ifdef DELETED
    /* Statistics are being updated during the blocked phase. */
    while (IS_BLOCKED(&self->sbox))
    {
        pthread_cond_wait(&self->sbox.update, &self->sbox.mutex);
    }
#endif /* DELETED */
    
    /* Make a dictionary object for organizing statistics */
    PyObject * result = PyDict_New();
    if (result == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        V(&self->sbox.mutex);
        FUNC_RET("%p", NULL);
    }
    
    /* Temporary variable(s) */
    PyObject * o = NULL;
    
    /* struct timespec to msec conversion */
    #ifndef ts2ms
    #define ts2ms(a) ((((a).tv_sec) * 1000) + (((a).tv_nsec) / 1000000))
    #endif
    
    PyDict_SetItemString(result, "elapsed", o = Py_BuildValue("k", \
        ts2ms(self->sbox.stat.elapsed)));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "cpu_info", o = Py_BuildValue("(k,k,k,K)", \
        ts2ms(self->sbox.stat.cpu_info.clock), \
        ts2ms(self->sbox.stat.cpu_info.utime), \
        ts2ms(self->sbox.stat.cpu_info.stime), \
        self->sbox.stat.cpu_info.tsc));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "mem_info", o = Py_BuildValue("(k,k,k,k,k,k)", 
        self->sbox.stat.mem_info.vsize / 1024,
        self->sbox.stat.mem_info.vsize_peak / 1024, 
        self->sbox.stat.mem_info.rss / 1024,
        self->sbox.stat.mem_info.rss_peak / 1024,
        self->sbox.stat.mem_info.minflt,
        self->sbox.stat.mem_info.majflt));
    Py_DECREF(o);
    
    syscall_t sc = *(syscall_t *)&self->sbox.stat.syscall;
    
    PyDict_SetItemString(result, "syscall_info", 
#ifdef __x86_64__
        o = Py_BuildValue("(i,i)", sc.scno, sc.mode));
#else
        o = Py_BuildValue("(i,i)", (long)sc, 0));
#endif /* __x86_64__ */
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "signal_info", 
        o = Py_BuildValue("(i,i)", self->sbox.stat.signal.signo,
                                   self->sbox.stat.signal.code));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "exitcode", 
        o = Py_BuildValue("i", self->sbox.stat.exitcode));
    Py_DECREF(o);
    
    /* The following fields are available from cpu_info and mem_info, and are
     * no longer maintained by the probe() method of the _sandbox.Sandbox class
     * in C module. For backward compatibility, sandbox.__init__.py provides a
     * derived Sandbox class, whose probe() method can generate these fields 
     * on-the-fly (except mem.nswap). */
    
#ifdef DELETED
    PyDict_SetItemString(result, "cpu", o = Py_BuildValue("k",
        self->sbox.stat.ru.ru_utime.tv_sec * 1000
        + self->sbox.stat.ru.ru_utime.tv_usec / 1000
        + self->sbox.stat.ru.ru_stime.tv_sec * 1000
        + self->sbox.stat.ru.ru_stime.tv_usec / 1000));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "cpu.usr", o = Py_BuildValue("k",
        self->sbox.stat.ru.ru_utime.tv_sec * 1000
        + self->sbox.stat.ru.ru_utime.tv_usec / 1000));
    Py_DECREF(o);

    PyDict_SetItemString(result, "cpu.sys", o = Py_BuildValue("k",
        self->sbox.stat.ru.ru_stime.tv_sec * 1000
        + self->sbox.stat.ru.ru_stime.tv_usec / 1000));
    Py_DECREF(o);

    PyDict_SetItemString(result, "cpu.tsc", o = Py_BuildValue("K",
        self->sbox.stat.tsc));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "mem.vsize_peak",
        o = Py_BuildValue("k", self->sbox.stat.vsize_peak / 1024));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.rss_peak",
        o = Py_BuildValue("k", self->sbox.stat.rss_peak / 1024));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.vsize",
        o = Py_BuildValue("k", self->sbox.stat.vsize / 1024));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.rss",
        o = Py_BuildValue("k", self->sbox.stat.rss / 1024));
    Py_DECREF(o);
    
    PyDict_SetItemString(result, "mem.minflt",
        o = Py_BuildValue("k", self->sbox.stat.ru.ru_minflt));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.majflt",
        o = Py_BuildValue("k", self->sbox.stat.ru.ru_majflt));
    Py_DECREF(o);

    PyDict_SetItemString(result, "mem.nswap",
        o = Py_BuildValue("k", self->sbox.stat.ru.ru_nswap));
    Py_DECREF(o);
#endif /* DELETED */
    
    V(&self->sbox.mutex);
    
    FUNC_RET("%p", result);
}

static PyObject *
Sandbox_dump(Sandbox * self, PyObject * args)
{
    FUNC_BEGIN("%p,%p", self, args);
    assert(self && args);

    P(&self->sbox.mutex);
    
    /* Cannot dump a process unless it is blocked */
    if (!IS_BLOCKED(&self->sbox))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_DUMP_NOT_BLOCKED);
        V(&self->sbox.mutex);
        FUNC_RET("%p", NULL);
    }
    
    /* Parse input arguments */
    int type = T_LONG;          /* expected datatype */
    unsigned long addr = 0;     /* address of targeted data */
    if (!PyArg_ParseTuple(args, "ik", &type, &addr))
    {
        V(&self->sbox.mutex);
        FUNC_RET("%p", NULL);
    }
    
    /* Empty object for storing dumped data */
    PyObject * result = NULL;
    
    /* Temporary variable(s) */
    char proc[4096] = {0};      /* we don't know about proc_t internals */
    long data = 0;
    
    /* Prototypes for calling private proc_* routines in libsandbox */
    bool proc_bind(const void * const, void * const);
    bool proc_probe(pid_t, int, void * const);
    bool proc_dump(const void * const, const void * const, long * const);
    
    proc_bind((void *)&self->sbox, (void *)&proc);
    
    if (!proc_probe(self->sbox.ctrl.pid, 0, (void *)&proc))
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_DUMP_PROBE_FAILED);
        V(&self->sbox.mutex);
        FUNC_RET("%p", NULL);
    }
    
    /* Dump data from the prisoner process */
    if (!proc_dump((const void *)&proc, (const void *)addr, &data))
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_DUMP_DUMP_FAILED);
        V(&self->sbox.mutex);
        FUNC_RET("%p", NULL);
    }
    
    switch (type)
    {
    case T_CHAR:
    case T_BYTE:
        result = PyLong_FromLong((long)(char)data);
        break;
    case T_UBYTE:
        result = PyLong_FromLong((long)(unsigned char)data);
        break;
    case T_SHORT:
        result = PyLong_FromLong((long)(short)data);
        break;
    case T_USHORT:
        result = PyLong_FromLong((long)(unsigned short)data);
        break;
    case T_FLOAT:
        result = PyFloat_FromDouble((double)(float)data);
        break;
    case T_DOUBLE:
        result = PyFloat_FromDouble((double)data);
        break;
    case T_INT:
        result = PyLong_FromLong((long)(int)data);
        break;
    case T_UINT:
        result = PyLong_FromUnsignedLong((unsigned long)(unsigned int)data);
        break;
    case T_LONG:
        result = PyLong_FromLong((long)data);
        break;
    case T_ULONG:
        result = PyLong_FromUnsignedLong((unsigned long)data);
        break;
    case T_STRING:
        result = PyBytes_FromString("");
        if (result != NULL)
        {
            while (true)
            {
                const char * ch = (const char *)(&data);
                unsigned int i = 0;
                for (i = 0; i < sizeof(data) / sizeof(char); i++)
                {
                    if (ch[i] == '\0')
                    {
                        goto fin_dump;
                    }
                    PyBytes_ConcatAndDel(&result, 
                        PyBytes_FromFormat("%c", ch[i]));
                    addr++;
                }
                if (!proc_dump((const void *)&proc, (const void *)addr, &data))
                {
                    Py_XDECREF(result);
                    result = NULL;
                    PyErr_SetString(PyExc_RuntimeError, MSG_DUMP_DUMP_FAILED);
                    goto fin_dump;
                }
            }
        fin_dump:
            ;
        }
        else
        {
            PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        }
        break;
    default:
        PyErr_SetString(PyExc_ValueError, MSG_ARGS_INVALID);
        result = Py_None;
        Py_INCREF(Py_None);
        break;
    }
    
    V(&self->sbox.mutex);
    
    FUNC_RET("%p", result);
}

static PyObject *
Sandbox_run(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);

    if (!sandbox_check(&self->sbox))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_SBOX_CHECK_FAILED);
        FUNC_RET("%p", NULL);
    }
    
    if (!SandboxPolicy_Check((PyObject *)self->sbox.ctrl.policy.data))
    {
        PyErr_SetString(PyExc_AssertionError, MSG_CTRL_CHECK_FAILED);
        FUNC_RET("%p", NULL);
    }
    
    sandbox_execute(&self->sbox);
    
    Py_INCREF(Py_None);
    FUNC_RET("%p", Py_None);
}

#ifdef DELETED

static PyObject *
Sandbox_start(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    /* TODO */
    PyErr_SetString(PyExc_NotImplementedError, MSG_NO_IMPL);
    FUNC_RET("%p", NULL);
}

static PyObject *
Sandbox_wait(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    /* TODO */
    PyErr_SetString(PyExc_NotImplementedError, MSG_NO_IMPL);
    FUNC_RET("%p", NULL);
}

static PyObject *
Sandbox_stop(Sandbox * self)
{
    FUNC_BEGIN("%p", self);
    assert(self);
    /* TODO */
    PyErr_SetString(PyExc_NotImplementedError, MSG_NO_IMPL);
    FUNC_RET("%p", NULL);
}

#endif /* DELETED */

/* The sandbox module */

#ifdef DELETED
static PyObject * sandbox_conv(Sandbox *, PyObject *);
#endif /* DELETED */

static PyMethodDef module_methods[] = {
#ifdef DELETED
    {"conv", (PyCFunction)sandbox_conv, METH_VARARGS,
     "Convert raw event data to specified Python native data type"},
#endif /* DELETED */
    {NULL, NULL, 0, NULL}      /* Sentinel */
};

#ifdef DELETED

static PyObject *
sandbox_conv(Sandbox * self, PyObject * args)
{
    FUNC_BEGIN("%p,%p", self, args);
    assert(self && args);

    /* Parse input arguments */
    int type = T_INT;           /* expected datatype */
    unsigned long value = 0;    /* raw value to convert */
    if (!PyArg_ParseTuple(args, "ik", &type, &value))
    {
        FUNC_RET("%p", NULL);
    }

    /* Empty object for storing converted data */
    PyObject * result = NULL;

    switch (type)
    {
    case T_BYTE:
    case T_UBYTE:
    case T_SHORT:
    case T_USHORT:
    case T_CHAR:
    case T_STRING:
        PyErr_SetString(PyExc_NotImplementedError, MSG_NO_IMPL);
        result = Py_None;
        Py_INCREF(Py_None);
        break;
    case T_INT:
    case T_LONG:
        result = PyInt_FromLong((long)value);
        break;
    case T_UINT:
    case T_ULONG:
        result = PyLong_FromUnsignedLong(value);
        break;
    case T_FLOAT:
    case T_DOUBLE:
        result = PyFloat_FromDouble((double)value);
        break;
    default:
        PyErr_SetString(PyExc_ValueError, MSG_ARGS_INVALID);
        result = Py_None;
        Py_INCREF(Py_None);
        break;
    }

    FUNC_RET("%p", result);
}

#endif /* DELETED */

#ifdef IS_PY3K

struct module_state 
{
    PyObject * error;
};

#define MODULE_STATE(m) ((struct module_state *)PyModule_GetState(m))

static int 
module_traverse(PyObject * m, visitproc visit, void * arg) 
{
    Py_VISIT(MODULE_STATE(m)->error);
    return 0;
}

static int 
module_clear(PyObject * m) 
{
    Py_CLEAR(MODULE_STATE(m)->error);
    return 0;
}

static struct PyModuleDef module_def = 
{
        PyModuleDef_HEAD_INIT,
        "_sandbox",
        NULL,
        sizeof(struct module_state),
        module_methods,
        NULL,
        module_traverse,
        module_clear,
        NULL
};

#define MODULE_INIT(x) PyObject * PyInit_ ## x (void)
#define INIT_BEGIN FUNC_BEGIN
#define INIT_RET(m) FUNC_RET("%p", m)

#else /* Python 2 */

#define MODULE_INIT(x) void init ## x (void)
#define INIT_BEGIN PROC_BEGIN
#define INIT_RET(m) PROC_END()

#endif /* IS_PY3K */

MODULE_INIT(_sandbox)
{
    INIT_BEGIN();
    
    PyObject * o = NULL;
    
    /* Initialize the sandbox module */
#ifdef IS_PY3K
    PyObject * module = PyModule_Create(&module_def);
#else /* Python 2 */
    PyObject * module = Py_InitModule3("_sandbox", module_methods, 
        "sandbox module");
#endif /* IS_PY3K */
    
    if (module == NULL)
    {
        INIT_RET(NULL);
    }
    
#ifdef IS_PY3K
    struct module_state *st = MODULE_STATE(module);
    st->error = PyErr_NewException("_sandbox.Error", NULL, NULL);
    if (st->error == NULL)
    {
        Py_DECREF(module);
        INIT_RET(NULL);
    }
#endif /* IS_PY3K */
    
    /* Package version */
    if (PyObject_SetAttrString(module, "__version__", 
        o = Py_BuildValue("s", VERSION)) != 0)
    {
        Py_DECREF(o);
        PyErr_SetString(PyExc_RuntimeError, MSG_ATTR_ADD_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    Py_DECREF(o);
    
    if (PyObject_SetAttrString(module, "__author__",
        o = Py_BuildValue("s", AUTHOR)) != 0)
    {
        Py_DECREF(o);
        PyErr_SetString(PyExc_RuntimeError, MSG_ATTR_ADD_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    Py_DECREF(o);
    
    /* Prepare constant attributes for SandboxEventType */
    SandboxEventType.tp_dict = PyDict_New();
    if (SandboxEventType.tp_dict == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    
    /* Wrapper items for constants in event_type_t */
    PyDict_SetItemString(SandboxEventType.tp_dict, "S_EVENT_ERROR", 
        o = Py_BuildValue("i", S_EVENT_ERROR));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxEventType.tp_dict, "S_EVENT_EXIT", 
        o = Py_BuildValue("i", S_EVENT_EXIT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxEventType.tp_dict, "S_EVENT_SIGNAL", 
        o = Py_BuildValue("i", S_EVENT_SIGNAL));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxEventType.tp_dict, "S_EVENT_SYSCALL", 
        o = Py_BuildValue("i", S_EVENT_SYSCALL));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxEventType.tp_dict, "S_EVENT_SYSRET", 
        o = Py_BuildValue("i", S_EVENT_SYSRET));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxEventType.tp_dict, "S_EVENT_QUOTA", 
        o = Py_BuildValue("i", S_EVENT_QUOTA));
    Py_DECREF(o);
    
    /* Finalize the sandbox event type */
    if (PyType_Ready(&SandboxEventType) != 0)
    {
        PyErr_SetString(PyExc_AssertionError, MSG_TYPE_READY_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    
    /* Add SandboxEventType to sandbox module */
    Py_INCREF(&SandboxEventType);
    if (PyModule_AddObject(module, "SandboxEvent", 
        (PyObject *)&SandboxEventType) != 0)
    {
        Py_DECREF(&SandboxEventType);
        PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    DBUG("added SandboxEventType to module");
    
    /* Prepare constant attributes for SandboxActionType */
    SandboxActionType.tp_dict = PyDict_New();
    if (SandboxActionType.tp_dict == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    
    /* Wrapper items for constants in action_type_t */
    PyDict_SetItemString(SandboxActionType.tp_dict, "S_ACTION_CONT", 
        o = Py_BuildValue("i", S_ACTION_CONT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxActionType.tp_dict, "S_ACTION_FINI", 
        o = Py_BuildValue("i", S_ACTION_FINI));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxActionType.tp_dict, "S_ACTION_KILL", 
        o = Py_BuildValue("i", S_ACTION_KILL));
    Py_DECREF(o);
    
    /* Finalize the sandbox action type */
    if (PyType_Ready(&SandboxActionType) != 0)
    {
        PyErr_SetString(PyExc_AssertionError, MSG_TYPE_READY_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    
    /* Add SandboxActionType to sandbox module */
    Py_INCREF(&SandboxActionType);
    if (PyModule_AddObject(module, "SandboxAction", 
        (PyObject *)&SandboxActionType) != 0)
    {
        Py_DECREF(&SandboxActionType);
        PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    DBUG("added SandboxActionType to module");
    
    /* Finalize the sandbox policy type */
    if (PyType_Ready(&SandboxPolicyType) != 0)
    {
        PyErr_SetString(PyExc_AssertionError, MSG_TYPE_READY_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    
    /* Add SandboxPolicyType to sandbox module */
    Py_INCREF(&SandboxPolicyType);
    if (PyModule_AddObject(module, "SandboxPolicy", 
        (PyObject *)&SandboxPolicyType) != 0)
    {
        Py_DECREF(&SandboxPolicyType);
        PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    DBUG("added SandboxPolicyType to module");
    
    /* Prepare constant attributes for SandboxType */
    SandboxType.tp_dict = PyDict_New();
    if (SandboxType.tp_dict == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, MSG_ALLOC_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    
    /* Wrapper items for constants in quota_type_t */
    PyDict_SetItemString(SandboxType.tp_dict, "S_QUOTA_WALLCLOCK", 
        o = Py_BuildValue("i", S_QUOTA_WALLCLOCK));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_QUOTA_CPU", 
        o = Py_BuildValue("i", S_QUOTA_CPU));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_QUOTA_MEMORY", 
        o = Py_BuildValue("i", S_QUOTA_MEMORY));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_QUOTA_DISK", 
        o = Py_BuildValue("i", S_QUOTA_DISK));
    Py_DECREF(o);
    
    /* Wrapper items for constants in status_t */
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_PRE", 
        o = Py_BuildValue("i", S_STATUS_PRE));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_RDY", 
        o = Py_BuildValue("i", S_STATUS_RDY));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_EXE", 
        o = Py_BuildValue("i", S_STATUS_EXE));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_BLK", 
        o = Py_BuildValue("i", S_STATUS_BLK));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_FIN", 
        o = Py_BuildValue("i", S_STATUS_FIN));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_S0",
        o = Py_BuildValue("i", S_STATUS_S0));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_S1",
        o = Py_BuildValue("i", S_STATUS_S1));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_STATUS_S2",
        o = Py_BuildValue("i", S_STATUS_S2));
    Py_DECREF(o);
    
    /* Wrapper items for constants in result_t */
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_PD", 
        o = Py_BuildValue("i", S_RESULT_PD));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_OK", 
        o = Py_BuildValue("i", S_RESULT_OK));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_RF", 
        o = Py_BuildValue("i", S_RESULT_RF));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_ML", 
        o = Py_BuildValue("i", S_RESULT_ML));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_OL", 
        o = Py_BuildValue("i", S_RESULT_OL));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_TL", 
        o = Py_BuildValue("i", S_RESULT_TL));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_RT", 
        o = Py_BuildValue("i", S_RESULT_RT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_AT", 
        o = Py_BuildValue("i", S_RESULT_AT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_IE", 
        o = Py_BuildValue("i", S_RESULT_IE));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_BP",
        o = Py_BuildValue("i", S_RESULT_BP));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_R0",
        o = Py_BuildValue("i", S_RESULT_R0));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_R1",
        o = Py_BuildValue("i", S_RESULT_R1));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_R2",
        o = Py_BuildValue("i", S_RESULT_R2));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_R3",
        o = Py_BuildValue("i", S_RESULT_R3));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_R4",
        o = Py_BuildValue("i", S_RESULT_R4));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "S_RESULT_R5",
        o = Py_BuildValue("i", S_RESULT_R5));
    Py_DECREF(o);

    /* Wrapper items for constants in structmembers.h */
    PyDict_SetItemString(SandboxType.tp_dict, "T_BYTE",
        o = Py_BuildValue("i", T_BYTE));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_SHORT",
        o = Py_BuildValue("i", T_SHORT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_INT",
        o = Py_BuildValue("i", T_INT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_LONG",
        o = Py_BuildValue("i", T_LONG));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_UBYTE",
        o = Py_BuildValue("i", T_UBYTE));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_USHORT",
        o = Py_BuildValue("i", T_USHORT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_UINT",
        o = Py_BuildValue("i", T_UINT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_ULONG",
        o = Py_BuildValue("i", T_ULONG));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_FLOAT",
        o = Py_BuildValue("i", T_FLOAT));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_DOUBLE",
        o = Py_BuildValue("i", T_DOUBLE));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_STRING",
        o = Py_BuildValue("i", T_STRING));
    Py_DECREF(o);
    PyDict_SetItemString(SandboxType.tp_dict, "T_CHAR",
        o = Py_BuildValue("i", T_CHAR));
    Py_DECREF(o);

    /* Finalize the sandbox policy type */
    if (PyType_Ready(&SandboxType) != 0)
    {
        PyErr_SetString(PyExc_AssertionError, MSG_TYPE_READY_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    
    /* Add SandboxType to sandbox module */
    Py_INCREF(&SandboxType);
    if (PyModule_AddObject(module, "Sandbox", (PyObject *)&SandboxType) != 0)
    {
        Py_DECREF(&SandboxType);
        PyErr_SetString(PyExc_RuntimeError, MSG_TYPE_ADD_FAILED);
        Py_DECREF(module);
        INIT_RET(NULL);
    }
    DBUG("added SandboxType to module");
    
    INIT_RET(module);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
