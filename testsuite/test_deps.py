#!/usr/bin/env python
################################################################################
# The Sandbox Libraries (Python) Test Suite 0 (Dependencies)                   #
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

__all__ = ['TestPlatformToolChain', 'TestPackageIntegrity', ]

import unittest


class TestPlatformToolChain(unittest.TestCase):

    def test_platform(self):
        from platform import system, machine
        self.assertTrue(system() in ('Linux', ))
        self.assertTrue(machine() in ('i686', 'x86_64', ))
        from sys import version_info
        self.assertTrue(version_info.major == 2 and version_info.minor >= 6 or
                        version_info.major >= 3)
        pass

    def test_toolchain(self):
        pass

    pass


class TestPackageIntegrity(unittest.TestCase):

    def test_package(self):
        import sandbox
        self.assertTrue(sandbox.__version__ >= "0.3.5-1")
        self.assertTrue(isinstance(sandbox.SandboxEvent, object))
        self.assertTrue(isinstance(sandbox.SandboxAction, object))
        self.assertTrue(isinstance(sandbox.SandboxPolicy, object))
        self.assertTrue(isinstance(sandbox.Sandbox, object))
        pass

    def test_policy_alloc(self):
        import sandbox
        e = sandbox.SandboxEvent(sandbox.S_EVENT_EXIT)
        self.assertTrue(hasattr(e, 'type'))
        self.assertTrue(hasattr(e, 'data'))
        a = sandbox.SandboxAction(sandbox.S_ACTION_CONT)
        self.assertTrue(hasattr(a, 'type'))
        self.assertTrue(hasattr(a, 'data'))
        p = sandbox.SandboxPolicy()
        self.assertTrue(callable(p))
        self.assertTrue(isinstance(p(e, a), sandbox.SandboxAction))
        self.assertEqual(p(e, a).type, sandbox.S_ACTION_FINI)
        pass

    def test_sandbox_alloc_err(self):
        import sandbox
        self.assertRaises(ValueError, sandbox.Sandbox, "/non/existence")
        pass

    def test_sandbox_alloc(self):
        import sandbox
        echo = (b"/bin/echo", b"Hello", b"World", )
        s = sandbox.Sandbox(echo)
        self.assertTrue(hasattr(s, 'task'))
        self.assertEqual(s.task, echo)
        self.assertTrue(hasattr(s, 'jail'))
        self.assertEqual(s.jail, b"/")
        self.assertTrue(hasattr(s, 'status'))
        self.assertEqual(s.status, sandbox.S_STATUS_RDY)
        self.assertTrue(hasattr(s, 'result'))
        self.assertEqual(s.result, sandbox.S_RESULT_PD)
        self.assertTrue(hasattr(s, 'policy'))
        self.assertTrue(isinstance(s.policy, sandbox.SandboxPolicy))
        self.assertTrue(hasattr(s, 'run'))
        self.assertTrue(callable(s.run))
        self.assertTrue(hasattr(s, 'probe'))
        self.assertTrue(callable(s.probe))
        pass

    pass


def test_suite():
    return unittest.TestSuite([
        unittest.TestLoader().loadTestsFromTestCase(TestPlatformToolChain),
        unittest.TestLoader().loadTestsFromTestCase(TestPackageIntegrity), ])

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
