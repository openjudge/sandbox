################################################################################
# The Sandbox Libraries (Python) Test Suite 2 (Quota Limitation)               #
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

__all__ = ['TestTimeQuota', 'TestMemoryQuota', 'TestOutputQuota', ]

import os

from sandbox import Sandbox

try:
    from . import config
    from .config import unittest
    from .policy import *
except (ValueError, ImportError):
    import config
    from config import unittest
    from policy import *


class TestTimeQuota(unittest.TestCase):

    def setUp(self):
        self.task = []
        self.task.append(config.build("busy_loop", config.CODE_LOOP))
        self.task.append(config.build("blocking_io", config.CODE_A_PLUS_B))
        self.task.append(config.build("pause", config.CODE_PAUSE))
        self.task.append(config.build("sleep", config.CODE_SLEEP))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_tl_busy_loop_500ms(self):
        s = Sandbox(self.task[0], quota=dict(wallclock=60000, cpu=500))
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_TL)
        d = s.probe(False)
        cpu = d['cpu_info'][0]
        eps = config.MAX_CPU_OVERRUN
        self.assertLess(s.quota[Sandbox.S_QUOTA_CPU], cpu)
        self.assertLess(cpu - s.quota[Sandbox.S_QUOTA_CPU], eps)
        pass

    def test_tl_busy_loop_1s(self):
        s = Sandbox(self.task[0], quota=dict(wallclock=60000, cpu=1000))
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_TL)
        d = s.probe(False)
        cpu = d['cpu_info'][0]
        eps = config.MAX_CPU_OVERRUN
        self.assertLess(s.quota[Sandbox.S_QUOTA_CPU], cpu)
        self.assertLess(cpu - s.quota[Sandbox.S_QUOTA_CPU], eps)
        pass

    def test_tl_blocking_io(self):
        s_rd, s_wr = os.pipe()
        s = Sandbox(self.task[1], quota=dict(wallclock=1000, cpu=2000), stdin=s_rd)
        s.policy = MinimalPolicy()
        s.run()
        os.fdopen(s_rd, "rb").close()
        os.fdopen(s_wr, "wb").close()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_TL)
        d = s.probe(False)
        elapsed = d['elapsed']
        eps = config.MAX_WALLCLOCK_OVERRUN
        self.assertLess(s.quota[Sandbox.S_QUOTA_WALLCLOCK], elapsed)
        self.assertLess(elapsed - s.quota[Sandbox.S_QUOTA_WALLCLOCK], eps)
        pass

    def test_tl_pause(self):
        s = Sandbox(self.task[2], quota=dict(wallclock=1000, cpu=2000))
        s.policy = AllowPauseSleepPolicy(s)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_TL)
        d = s.probe(False)
        elapsed = d['elapsed']
        eps = config.MAX_WALLCLOCK_OVERRUN
        self.assertLess(s.quota[Sandbox.S_QUOTA_WALLCLOCK], elapsed)
        self.assertLess(elapsed - s.quota[Sandbox.S_QUOTA_WALLCLOCK], eps)
        pass

    def test_tl_sleep(self):
        s = Sandbox(self.task[3], quota=dict(wallclock=1000, cpu=2000))
        s.policy = AllowPauseSleepPolicy(s)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_TL)
        d = s.probe(False)
        elapsed = d['elapsed']
        eps = config.MAX_WALLCLOCK_OVERRUN
        self.assertLess(s.quota[Sandbox.S_QUOTA_WALLCLOCK], elapsed)
        self.assertLess(elapsed - s.quota[Sandbox.S_QUOTA_WALLCLOCK], eps)
        pass

    pass


class TestMemoryQuota(unittest.TestCase):

    def setUp(self):
        self.task = []
        self.task.append(config.build("mem_static", config.CODE_MEM_STATIC))
        self.task.append(config.build("mem_alloc", config.CODE_MEM_ALLOC))
        for t in self.task:
            self.assertTrue(t is not None)
        pass

    def test_ml_static(self):
        s = Sandbox(self.task[0], quota=dict(wallclock=60000, cpu=2000, memory=2 ** 24))
        s.policy = MinimalPolicy()
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_ML)
        d = s.probe(False)
        mem = d['mem_info'][1] * 1024
        self.assertLess(s.quota[Sandbox.S_QUOTA_MEMORY], mem)
        pass

    def test_ml_alloc(self):
        s_wr = open("/dev/null", "wb")
        s = Sandbox(self.task[1], quota=dict(wallclock=60000, cpu=2000, memory=2 ** 24),
                    stderr=s_wr)
        s.policy = MinimalPolicy()
        s.run()
        s_wr.close()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_ML)
        d = s.probe(False)
        mem = d['mem_info'][1] * 1024
        self.assertLess(s.quota[Sandbox.S_QUOTA_MEMORY], mem)
        pass

    pass


class TestOutputQuota(unittest.TestCase):

    def setUp(self):
        self.fout = []
        self.task = []
        self.fout.append(config.touch("file_write.out"))
        self.task.append(config.build("file_write", config.TMPL_FILE_WRITE.replace(
            b"@FN@", ("\"%s\"" % self.fout[-1]).encode())))
        self.fout.append(config.touch("loop_print.out"))
        self.task.append(config.build("loop_print", config.CODE_LOOP_PRINT))
        for fn in self.fout + self.task:
            self.assertTrue(fn is not None)
        pass

    def test_ol_file_write(self):
        s = Sandbox(self.task[0], quota=dict(wallclock=60000, cpu=2000, disk=5))
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OL)
        with open(self.fout[0], "rb") as f:
            self.assertEqual(f.read(), b"Hello")
            f.close()
        pass

    def test_ol_redirected(self):
        s_wr = open(self.fout[1], "wb")
        s = Sandbox(self.task[1], quota=dict(wallclock=60000, cpu=2000, disk=5),
                    stdout=s_wr)
        s.run()
        s_wr.close()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OL)
        with open(self.fout[1], "rb") as f:
            self.assertEqual(f.read(), b"Hello")
            f.close()
        pass

    pass


def test_suite():
    return unittest.TestSuite([
        unittest.TestLoader().loadTestsFromTestCase(eval(c)) for c in __all__])

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
