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

from __future__ import with_statement

__all__ = ['TestInputOutput', ]

import unittest


class TestInputOutput(unittest.TestCase):

    def test_piped_stdin(self):
        import os
        from sandbox import Sandbox
        from subprocess import Popen
        # /bin/echo "Hello World" | sbox /bin/cat
        p_rd, p_wr = os.pipe()
        p = Popen(["/bin/echo", "Hello", "World"], close_fds=True, stdout=p_wr)
        os.fdopen(p_wr, 'wb').close()
        s_rd, s_wr = os.pipe()
        s = Sandbox("/bin/cat", stdin=p_rd, stdout=s_wr)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OK)
        # close the write-end of the pipe before reading from the read-end
        os.fdopen(s_wr, 'wb').close()
        with os.fdopen(s_rd, 'rb') as f:
            self.assertEqual(f.read(), b"Hello World\n")
            f.close()
        pass

    def test_piped_stdout(self):
        import os
        from sandbox import Sandbox
        from subprocess import Popen, PIPE
        # sbox /bin/echo "Hello World" | /bin/cat
        s_rd, s_wr = os.pipe()
        s = Sandbox(["/bin/echo", "Hello", "World"], stdout=s_wr)
        p = Popen("/bin/cat", close_fds=True, stdin=s_rd, stdout=PIPE)
        s.run()
        self.assertEqual(s.status, Sandbox.S_STATUS_FIN)
        self.assertEqual(s.result, Sandbox.S_RESULT_OK)
        # close the write-end of the pipe before reading from the read-end
        os.fdopen(s_wr, 'wb').close()
        os.fdopen(s_rd, 'rb').close()
        stdout, stderr = p.communicate()
        self.assertEqual(stdout, b"Hello World\n")
        pass

    pass


def test_suite():
    return unittest.TestSuite([
        unittest.TestLoader().loadTestsFromTestCase(TestInputOutput), ])

if __name__ == '__main__':
    unittest.main(defaultTest='test_suite')
