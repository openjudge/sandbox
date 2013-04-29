################################################################################
# The Sandbox Libraries (Python) Test Suite 5 (Syscall Tracing)                #
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
#

from __future__ import with_statement

__all__ = ['TestMemoryDump', 'TestSyscallMode', 'TestExec', 'TestMultiProcessing', ]

import os
import sys

from platform import machine
from posix import O_RDONLY
from sandbox import Sandbox

try:
    from . import config
    from .config import unittest
    from .policy import *
except (ValueError, ImportError):
    import config
    from config import unittest
    from policy import *


class TestMemoryDump(unittest.TestCase):

    def setUp(self):
        self.task = []
        for fn, ptr in zip(("open", "open_null", "open_bogus"),
                           (b"\"/dev/zero\"", b"NULL", b"(char *)0x01")):
            self.task.append(
                config.build(fn, config.TMPL_FILE_READ.replace(b"@FN@", ptr)))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_open(self):
        s = Sandbox(self.task[0])
        s.policy = SelectiveOpenPolicy(s, set([(b"/dev/zero", O_RDONLY), ]))
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OK)
        pass

    def test_open_null(self):
        # dumping NULL address should cause RT not BP/IE
        s = Sandbox(self.task[1])
        s.policy = SelectiveOpenPolicy(s, set())
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        sc = d['syscall_info'] if machine() == 'x86_64' else d['syscall_info'][0]
        self.assertTrue(sc in SelectiveOpenPolicy.SC_open)
        pass

    def test_open_bogus(self):
        # dumping bad address should cause RT not BP/IE
        s = Sandbox(self.task[2])
        s.policy = SelectiveOpenPolicy(s, set())
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        sc = d['syscall_info'] if machine() == 'x86_64' else d['syscall_info'][0]
        self.assertTrue(sc in SelectiveOpenPolicy.SC_open)
        pass

    pass


@unittest.skipIf(machine() != 'x86_64', "test requires platforms with multiple abi's")
class TestSyscallMode(unittest.TestCase):

    def setUp(self):
        self.task = []
        self.task.append(config.build("int80_exit1", config.CODE_INT80_EXIT1))
        self.task.append(config.build("int80_fork", config.CODE_INT80_FORK))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_int80_exit1(self):
        # syscall #1: (exit, i686) vs (write, x86_64)
        s = Sandbox(self.task[0])
        s.policy = AllowExitPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_AT)
        d = s.probe(False)
        self.assertEqual(d['exitcode'], 1)
        self.assertEqual(d['syscall_info'], (1, 1))
        pass

    def test_int80_fork(self):
        # syscall #2: (fork, i686) vs (open, x86_64)
        s = Sandbox(self.task[1])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        d = s.probe(False)
        self.assertEqual(d['syscall_info'], (2, 1))
        pass

    pass


class TestExec(unittest.TestCase):

    def setUp(self):
        self.task = []
        self.task.append(config.build("exec", config.CODE_EXEC))
        self.task.append(config.build("hello", config.CODE_HELLO_WORLD))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_exec_rf(self):
        # minimal policy forbids execve()
        s = Sandbox(self.task[:])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        d = s.probe(False)
        sc = d['syscall_info'] if machine() == 'x86_64' else d['syscall_info'][0]
        self.assertTrue(sc in AllowExecOncePolicy.SC_execve)
        pass

    def test_exec(self):
        # single execve(): ./exec.exe ./hello.exe
        s_rd, s_wr = os.pipe()
        s = Sandbox(self.task[:], stdout=s_wr)
        s.policy = AllowExecOncePolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OK)
        os.fdopen(s_wr, 'wb').close()
        with os.fdopen(s_rd, 'rb') as f:
            self.assertEqual(f.read(), b"Hello World!\n")
            f.close()
        pass

    def test_exec_nested(self):
        # nested execve(): ./exec.exe ./exec.exe ./hello.exe
        s = Sandbox([self.task[0], ] + self.task[:])
        s.policy = AllowExecOncePolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        d = s.probe(False)
        sc = d['syscall_info'] if machine() == 'x86_64' else d['syscall_info'][0]
        self.assertTrue(sc in AllowExecOncePolicy.SC_execve)
        pass

    pass


class TestMultiProcessing(unittest.TestCase):

    def setUp(self):
        self.task = []
        self.task.append(config.build("fork", config.CODE_FORK))
        self.task.append(config.build("vfork", config.CODE_VFORK))
        self.task.append(config.build("clone", config.CODE_PTHREAD, LDFLAGS="-pthread"))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_fork(self):
        s = Sandbox(self.task[0])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        pass

    def test_vfork(self):
        s = Sandbox(self.task[1])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        pass

    def test_clone(self):
        s = Sandbox(self.task[2])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        pass

    pass


def test_suite():
    return unittest.TestSuite([
        unittest.TestLoader().loadTestsFromTestCase(eval(c)) for c in __all__])

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
