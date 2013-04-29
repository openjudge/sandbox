################################################################################
# The Sandbox Libraries (Python) Test Suite 4 (Signal Detection)               #
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

__all__ = ['TestSignalSending', 'TestSignalReceiving', 'TestSignalHandling', ]

import os
import sys

from platform import machine
from signal import (SIGABRT, SIGBUS, SIGFPE, SIGKILL, SIGSEGV, SIGTERM, SIGTRAP,
                    SIGXCPU, SIGXFSZ)
from sandbox import Sandbox

# si_code from <bits/siginfo.h>
SI_USER = 0
SI_KERNEL = 0x80
SI_TKILL = -6
BUS_ADRALN = 1
FPE_INTDIV = 1
SEGV_MAPERR = 1
SEGV_ACCERR = 2

try:
    from . import config
    from .config import unittest
    from .policy import *
except (ValueError, ImportError):
    import config
    from config import unittest
    from policy import *


class TestSignalSending(unittest.TestCase):

    def setUp(self):
        self.task = []
        self.task.append(config.build(
            "kill_ppid", config.TMPL_KILL_PPID.replace(b"@SIG@", b"SIGKILL")))
        self.task.append(config.build("assert", config.CODE_SIGABRT))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_kill_ppid(self):
        s = Sandbox(self.task[0])
        s.policy = AllowSelfKillPolicy(s)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        d = s.probe(False)
        sc = d['syscall_info'] if machine() == 'x86_64' else d['syscall_info'][0]
        self.assertTrue(sc in AllowSelfKillPolicy.SC_kill)
        pass

    def test_assert_rf(self):
        s_wr = open("/dev/null", "wb")
        s = Sandbox(self.task[1], stderr=s_wr)
        s.policy = MinimalPolicy()
        s.run()
        s_wr.close()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RF)
        d = s.probe(False)
        sc = d['syscall_info'] if machine() == 'x86_64' else d['syscall_info'][0]
        self.assertTrue(sc in AllowSelfKillPolicy.SC_rt_sigprocmask or
                        sc in AllowSelfKillPolicy.SC_sigprocmask)
        pass

    def test_assert(self):
        s_wr = open("/dev/null", "wb")
        s = Sandbox(self.task[1], stderr=s_wr)
        s.policy = AllowSelfKillPolicy(s)
        s.run()
        s_wr.close()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGABRT, SI_TKILL))
        sc = d['syscall_info'] if machine() == 'x86_64' else d['syscall_info'][0]
        self.assertTrue(sc in AllowSelfKillPolicy.SC_tgkill)
        pass

    pass


class TestSignalReceiving(unittest.TestCase):

    def setUp(self):
        self.task = []
        self.task.append(config.build("sigbus_adraln", config.CODE_SIGBUS_ADRALN,
                                      CFLAGS=config.CFLAGS + " -ansi"))
        self.task.append(config.build("sigfpe_intdiv", config.CODE_SIGFPE_INTDIV))
        self.task.append(config.build("sigsegv_maperr", config.CODE_SIGSEGV_MAPERR))
        self.task.append(config.build("sigsegv_accerr", config.CODE_SIGSEGV_ACCERR))
        self.task.append(config.build("busy_loop", config.CODE_LOOP))
        self.task.append(config.build("rlimit_fsz", config.CODE_SIGXFSZ))
        self.task.append(config.build("rlimit_cpu", config.CODE_SIGXCPU))
        self.task.append(config.build("a_plus_b", config.CODE_A_PLUS_B))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_sigbus_adraln(self):
        s = Sandbox(self.task[0])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGBUS, BUS_ADRALN))
        pass

    def test_sigfpe_intdiv(self):
        s = Sandbox(self.task[1])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGFPE, FPE_INTDIV))
        pass

    def test_sigsegv_maperr(self):
        s = Sandbox(self.task[2])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGSEGV, SEGV_MAPERR))
        pass

    def test_sigsegv_accerr(self):
        s = Sandbox(self.task[3])
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGSEGV, SEGV_ACCERR))
        pass

    def test_sigterm_user(self):
        s = Sandbox(self.task[4])
        s.policy = KillerPolicy(s, SIGTERM)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGTERM, SI_USER))
        pass

    def test_sigkill(self):
        s = Sandbox(self.task[4])
        s.policy = KillerPolicy(s, SIGKILL)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'][0], SIGKILL)
        pass

    def test_sigkill_io(self):
        s_rd, s_wr = os.pipe()
        s = Sandbox(self.task[7], stdin=s_rd)
        s.policy = KillerPolicy(s, SIGKILL)
        s.run()
        os.fdopen(s_rd, "rb").close()
        os.fdopen(s_wr, "wb").close()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'][0], SIGKILL)
        pass

    def test_sigtrap_user(self):
        s = Sandbox(self.task[4])
        s.policy = KillerPolicy(s, SIGTRAP)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGTRAP, SI_USER))
        pass

    def test_sigxfsz_user(self):
        s = Sandbox(self.task[4])
        s.policy = KillerPolicy(s, SIGXFSZ)
        s.run()
        # this results in OL instead of RT because the way linux file systems
        # notify disk quota limitation is simply sending SIGXFSZ to the program
        # unfortunately the si_code is SI_USER making it not distinguishable
        # from a synthetic SIGXFSZ signal sent from third party
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OL)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGXFSZ, SI_USER))
        pass

    def test_rlimit_fsz(self):
        s_wr = open(config.touch("rlimit_fsz.out"), "wb")
        s = Sandbox(self.task[5], stdout=s_wr)
        s.policy = AllowResLimitPolicy(s)
        s.run()
        s_wr.close()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OL)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGXFSZ, SI_USER))
        pass

    def test_rlimit_cpu(self):
        s = Sandbox(self.task[6])
        s.policy = AllowResLimitPolicy(s)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_RT)
        d = s.probe(False)
        self.assertEqual(d['signal_info'], (SIGXCPU, SI_KERNEL))
        pass

    pass


class TestSignalHandling(unittest.TestCase):

    def setUp(self):
        pass

    pass


def test_suite():
    return unittest.TestSuite([
        unittest.TestLoader().loadTestsFromTestCase(eval(c)) for c in __all__])

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
