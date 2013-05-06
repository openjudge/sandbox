################################################################################
# The Sandbox Libraries (Python) Test Suite 3 (System-Level Restrictions)      #
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

__all__ = ['TestSystemRestriction', ]

import os

from errno import EACCES
from posix import O_WRONLY, O_CREAT, O_TRUNC
from pwd import getpwnam
from sandbox import Sandbox

try:
    from . import config
    from .config import unittest
    from .policy import *
except (ValueError, ImportError):
    import config
    from config import unittest
    from policy import *


@unittest.skipIf(config.su('root') is None, "test requires root privilege")
class TestSystemRestriction(unittest.TestCase):

    def setUp(self):
        self.fout = []
        self.task = []
        self.secret = b"Magic World!"
        # 0: EACCES
        self.fout.append(config.touch("file_write_0.out", self.secret, 0o600))
        self.task.append(config.build("file_write_0", config.TMPL_FILE_WRITE.replace(
            b"@FN@", ("\"%s\"" % self.fout[-1]).encode())))
        # allow everyone to read from jail
        self.prefix = os.path.abspath(os.path.normpath(os.path.dirname(self.task[-1])))
        os.chmod(self.prefix, 0o755)
        self.addCleanup(os.chmod, self.prefix, 0o700)
        # 1: EACCES
        fn_rel = "../file_write_1.out"
        fn_full = config.touch(fn_rel, self.secret, 0o666)
        self.assertTrue(fn_full is not None)
        self.addCleanup(os.unlink, fn_full)
        self.fout.append(fn_rel)
        self.task.append(config.build("file_write_1", config.TMPL_FILE_WRITE.replace(
            b"@FN@", ("\"%s\"" % self.fout[-1]).encode())))
        # 2: OK
        fn_rel = "./file_write_2.out"
        fn_full = config.touch(fn_rel, self.secret, 0o666)
        self.assertTrue(fn_full is not None)
        self.fout.append(fn_rel)
        self.task.append(config.build("file_write_2", config.TMPL_FILE_WRITE.replace(
            b"@FN@", ("\"%s\"" % self.fout[-1]).encode())))
        for f in self.fout + self.task:
            self.assertTrue(f is not None)
        pass

    def test_owner_group(self):
        nobody = getpwnam('nobody')
        s = Sandbox(self.task[0], owner=nobody.pw_uid, group=nobody.pw_gid)
        s.policy = SelectiveOpenPolicy(s, [(self.fout[0].encode(), O_WRONLY | O_CREAT | O_TRUNC), ])
        self.assertEqual(s.owner, nobody.pw_uid)
        self.assertEqual(s.group, nobody.pw_gid)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_AT)
        d = s.probe(False)
        self.assertEqual(d['exitcode'], EACCES)
        # make sure file content is not changed
        with open(self.fout[0], "rb") as f:
            self.assertEqual(f.read(), self.secret)
            f.close()
        pass

    def test_chroot_jail(self):
        nobody = getpwnam('nobody')
        s = Sandbox(self.task[1], jail=self.prefix, owner=nobody.pw_uid, group=nobody.pw_gid)
        s.policy = SelectiveOpenPolicy(s, [(self.fout[1].encode(), O_WRONLY | O_CREAT | O_TRUNC), ])
        self.assertEqual(s.jail, self.prefix)
        self.assertEqual(s.owner, nobody.pw_uid)
        self.assertEqual(s.group, nobody.pw_gid)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_AT)
        d = s.probe(False)
        self.assertEqual(d['exitcode'], EACCES)
        # make sure file content is not changed
        with open(os.path.join(self.prefix, self.fout[1]), "rb") as f:
            self.assertEqual(f.read(), self.secret)
            f.close()
        pass

    def test_chroot_jail_ok(self):
        nobody = getpwnam('nobody')
        s = Sandbox(self.task[2], jail=self.prefix, owner=nobody.pw_uid, group=nobody.pw_gid)
        s.policy = SelectiveOpenPolicy(s, [(self.fout[2].encode(), O_WRONLY | O_CREAT | O_TRUNC), ])
        self.assertEqual(s.jail, self.prefix)
        self.assertEqual(s.owner, nobody.pw_uid)
        self.assertEqual(s.group, nobody.pw_gid)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OK)
        # make sure file content is not changed
        with open(os.path.join(self.prefix, self.fout[2]), "rb") as f:
            self.assertEqual(f.read(), b"Hello World!\n")
            f.close()
        pass

    pass


def test_suite():
    return unittest.TestSuite([
        unittest.TestLoader().loadTestsFromTestCase(eval(c)) for c in __all__])

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
