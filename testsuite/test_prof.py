#!/usr/bin/env python
################################################################################
# The Sandbox Libraries (Python) Test Suite 1 (Basic Profiling)                #
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

__all__ = ['TestSandboxedIO', ]

import unittest


class TestSandboxedIO(unittest.TestCase):

    def test_piped_stdout(self):
        import os
        import fcntl
        import sandbox
        # redirect the output of /bin/echo through OS-pipe
        p_rd, p_wr = os.pipe()
        f = os.fdopen(p_rd, 'rb')
        # set the reading-end of the pipe to be non-blocking
        fl = fcntl.fcntl(f.fileno(), fcntl.F_GETFL)
        fcntl.fcntl(f.fileno(), fcntl.F_SETFL, fl | os.O_NONBLOCK)
        # run /bin/echo within the sandbox
        s = sandbox.Sandbox(["/bin/echo", "Hello", "World"], stdout=p_wr)
        s.run()
        self.assertEqual(f.read(), b"Hello World\n")
        f.close()
        self.assertEqual(s.status, sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, sandbox.S_RESULT_OK)
        pass

    pass


def test_suite():
    return unittest.TestSuite([
        unittest.TestLoader().loadTestsFromTestCase(TestSandboxedIO), ])

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
