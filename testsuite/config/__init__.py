################################################################################
# The Sandbox Libraries (Python) Test Suite                                    #
#                                                                              #
# Copyright (C) 2004-2009, 2011-2013 LIU Yu, pineapple.liu@gmail.com           #
# All rights reserved.                                                         #
#                                                                              #
# Redistribution and use in source and binary forms, with or without           #
# modification, are permitted provided that the following conditions are met:  #
#                                                                              #
# 1. Redistributions of source code must retain the above copyright notice,    #
#    this list of conditions and the following disclaimer.                     #
#                                                                              #
# 2. Redistributions in binary form must reproduce the above copyright notice, #
#    this list of conditions and the following disclaimer in the documentation #
#    and/or other materials provided with the distribution.                    #
#                                                                              #
# 3. Neither the name of the author(s) nor the names of its contributors may   #
#    be used to endorse or promote products derived from this software without #
#    specific prior written permission.                                        #
#                                                                              #
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  #
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    #
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   #
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE     #
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          #
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         #
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     #
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      #
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      #
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE   #
# POSSIBILITY OF SUCH DAMAGE.                                                  #
################################################################################

from __future__ import with_statement

import os
import sys
import tempfile

try:
    # python 2.6
    import unittest2 as unittest
except ImportError:
    # python 2.7+
    import unittest

MAX_CPU_OVERRUN = int(os.environ.get("MAX_CPU_OVERRUN", "200"))  # < 0.2sec
MAX_WALLCLOCK_OVERRUN = int(os.environ.get("MAX_WALLCLOCK_OVERRUN", "2000"))  # < 2sec

CC = os.environ.get("CC", "gcc")
CFLAGS = os.environ.get("CFLAGS", "-Wall --static")
LDFLAGS = os.environ.get("LDFLAGS", "")


def su(usr):
    from pwd import getpwnam
    uid = None
    try:
        uid = usr if isinstance(usr, int) else getpwnam(usr).pw_uid
        os.seteuid(uid)
    except:
        uid = None
    return uid


TEMP_DIR = tempfile.mkdtemp()
DATA_DIR = os.path.dirname(__file__)


def load_data(fn):
    global DATA_DIR
    return open(os.path.join(DATA_DIR, fn), "rb").read()


CODE_HELLO_WORLD = load_data("hello.c")
CODE_A_PLUS_B = load_data("a_plus_b.c")
CODE_EXIT1 = load_data("exit1.c")
CODE_EXIT_GROUP1 = load_data("exit_group1.c")
TMPL_FILE_READ = load_data("file_read.c.in")
TMPL_FILE_WRITE = load_data("file_write.c.in")

CODE_LOOP = load_data("loop.c")
CODE_LOOP_PRINT = load_data("loop_print.c")
CODE_MEM_ALLOC = load_data("mem_alloc.c")
CODE_MEM_STATIC = load_data("mem_static.c")

CODE_SLEEP = load_data("sleep.c")
CODE_PAUSE = load_data("pause.c")
CODE_SIGXCPU = load_data("sigxcpu.c")
CODE_SIGXFSZ = load_data("sigxfsz.c")

TMPL_KILL_PPID = load_data("kill_ppid.c.in")
CODE_SIGABRT = load_data("sigabrt.c")
CODE_SIGBUS_ADRALN = load_data("sigbus_adraln.c")
CODE_SIGFPE_INTDIV = load_data("sigfpe_intdiv.c")
CODE_SIGSEGV_MAPERR = load_data("sigsegv_maperr.c")
CODE_SIGSEGV_ACCERR = load_data("sigsegv_accerr.c")

CODE_EXEC = load_data("exec.c")
CODE_FORK = load_data("fork.c")
CODE_VFORK = load_data("vfork.c")
CODE_PTHREAD = load_data("pthread.c")
CODE_INT80_EXIT1 = load_data("int80_exit1.c")
CODE_INT80_FORK = load_data("int80_fork.c")


def touch(fn, data=b"", mode=0o666):
    global TEMP_DIR
    from os import access, F_OK, R_OK, W_OK
    fn_full = os.path.join(TEMP_DIR, fn)
    with open(fn_full, "wb") as f:
        f.write(data)
        f.close()
    os.chmod(fn_full, mode)
    if not access(fn_full, F_OK | R_OK | W_OK):
        return None
    return fn_full


def build(fn, code, CC=CC, CFLAGS=CFLAGS, LDFLAGS=LDFLAGS):
    global TEMP_DIR
    from os import access, system, F_OK, R_OK, X_OK
    # write program code
    fn_src = touch(".".join([fn, "c"]), code)
    fn_exe = fn_src.rstrip(".c")
    # compile program
    cmd = " ".join([CC, CFLAGS, "-o", fn_exe, fn_src, LDFLAGS])
    if system(cmd) != 0:
        return None
    if not access(fn_exe, F_OK | R_OK | X_OK):
        return None
    return fn_exe


def cleanup():
    global TEMP_DIR
    import shutil
    shutil.rmtree(TEMP_DIR)
    pass


import atexit
atexit.register(cleanup)
