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

__all__ = ['MinimalPolicy', 'AllowExitPolicy', 'AllowExecOncePolicy',
           'AllowPauseSleepPolicy', 'AllowSelfKillPolicy', 'AllowResLimitPolicy',
           'SelectiveOpenPolicy', 'KillerPolicy', ]

import os
import sys
import signal
import threading

from platform import system, machine
from sandbox import *

if system() not in ('Linux', ) or machine() not in ('x86_64', 'i686', ):
    raise AssertionError("Unsupported platform type.\n")


class MinimalPolicy(SandboxPolicy):

    # white list of essential linux syscalls for statically-linked C programs
    sc_safe = {'i686': set([0, 3, 4, 19, 45, 54, 90, 91, 122, 125, 140, 163,
        192, 197, 224, 243, 252, ]), 'x86_64': set([0, 1, 5, 8, 9, 10, 11, 12,
        16, 25, 63, 158, 186, 219, 231, ])}

    def __init__(self, sbox=None):
        super(MinimalPolicy, self).__init__()
        # initialize table of system call rules
        self.sc_table = {}
        if machine() == 'x86_64':
            for (mode, abi) in ((0, 'x86_64'), (1, 'i686'), ):
                for scno in MinimalPolicy.sc_safe[abi]:
                    self.sc_table[(scno, mode)] = self._CONT
        else:  # i686
            for scno in MinimalPolicy.sc_safe[machine()]:
                self.sc_table[scno] = self._CONT
        # save a local reference to the parent sandbox
        if isinstance(sbox, Sandbox):
            self.sbox = sbox
        pass

    def __call__(self, e, a):
        # handle SYSCALL/SYSRET events with local rules
        if e.type in (S_EVENT_SYSCALL, S_EVENT_SYSRET):
            sc = (e.data, e.ext0) if machine() == 'x86_64' else e.data
            rule = self.sc_table.get(sc, self._KILL_RF)
            return rule(e, a)
        # bypass other events to base class
        return super(MinimalPolicy, self).__call__(e, a)

    def _CONT(self, e, a):  # continue
        a.type = S_ACTION_CONT
        return a

    def _KILL_RF(self, e, a):  # restricted func.
        a.type, a.data = S_ACTION_KILL, S_RESULT_RF
        return a

    def _KILL_RT(self, e, a):  # runtime error
        a.type, a.data = S_ACTION_KILL, S_RESULT_RT
        return a

    pass


class AllowExitPolicy(MinimalPolicy):

    SC_exit = ((60, 0), (1, 1), ) if machine() == 'x86_64' else (1, )

    def __init__(self):
        super(AllowExitPolicy, self).__init__()
        for sc in AllowExitPolicy.SC_exit:
            self.sc_table[sc] = self._CONT
        pass

    pass


class AllowExecOncePolicy(MinimalPolicy):

    SC_execve = ((59, 0), (11, 1), ) if machine() == 'x86_64' else (11, )

    def __init__(self):
        super(AllowExecOncePolicy, self).__init__()
        for sc in AllowExecOncePolicy.SC_execve:
            self.sc_table[sc] = self.SYS_execve
        self.execntd = 1
        pass

    def SYS_execve(self, e, a):
        if e.type == S_EVENT_SYSCALL:
            if self.execntd > 0:
                self.execntd -= 1
            else:
                return self._KILL_RF(e, a)
        return self._CONT(e, a)

    pass


class AllowSelfKillPolicy(MinimalPolicy):

    SC_getppid = ((110, 0), (64, 1), ) if machine() == 'x86_64' else (64, )
    SC_rt_sigaction = ((13, 0), (174, 1), ) if machine() == 'x86_64' else (174, )
    SC_sigaction = ((67, 1), ) if machine() == 'x86_64' else (67, )
    SC_rt_sigprocmask = ((14, 0), (175, 1), ) if machine() == 'x86_64' else (175, )
    SC_sigprocmask = ((126, 1), ) if machine() == 'x86_64' else (126, )
    SC_kill = ((62, 0), (37, 1), ) if machine() == 'x86_64' else (37, )
    SC_tkill = ((200, 0), (238, 1), ) if machine() == 'x86_64' else (238, )
    SC_tgkill = ((234, 0), (270, 1), ) if machine() == 'x86_64' else (270, )

    def __init__(self, sbox):
        super(AllowSelfKillPolicy, self).__init__(sbox)
        for sc in AllowSelfKillPolicy.SC_getppid +\
                AllowSelfKillPolicy.SC_rt_sigaction +\
                AllowSelfKillPolicy.SC_sigaction +\
                AllowSelfKillPolicy.SC_rt_sigprocmask +\
                AllowSelfKillPolicy.SC_sigprocmask:
            self.sc_table[sc] = self._CONT
        for sc in AllowSelfKillPolicy.SC_kill +\
                AllowSelfKillPolicy.SC_tkill +\
                AllowSelfKillPolicy.SC_tgkill:
            self.sc_table[sc] = self.SYS_tkill
        pass

    def SYS_tkill(self, e, a):  # kill / tkill / tgkill
        if e.type == S_EVENT_SYSCALL:
            if e.ext1 != self.sbox.pid:
                return self._KILL_RF(e, a)
        return self._CONT(e, a)

    pass


class AllowPauseSleepPolicy(AllowSelfKillPolicy):

    SC_pause = ((34, 0), (29, 1), ) if machine() == 'x86_64' else (29, )
    SC_nanosleep = ((35, 0), (162, 1), ) if machine() == 'x86_64' else (162, )

    def __init__(self, sbox):
        super(AllowPauseSleepPolicy, self).__init__(sbox)
        for sc in AllowPauseSleepPolicy.SC_pause +\
                AllowPauseSleepPolicy.SC_nanosleep:
            self.sc_table[sc] = self._CONT
        pass

    pass


class AllowResLimitPolicy(AllowSelfKillPolicy):

    SC_getrlimit = ((97, 0), (76, 1), ) if machine() == 'x86_64' else (76, )
    SC_setrlimit = ((160, 0), (75, 1), ) if machine() == 'x86_64' else (75, )
    SC_ugetrlimit = ((191, 1), ) if machine() == 'x86_64' else (191, )

    def __init__(self, sbox):
        super(AllowResLimitPolicy, self).__init__(sbox)
        for sc in AllowResLimitPolicy.SC_setrlimit +\
                AllowResLimitPolicy.SC_getrlimit +\
                AllowResLimitPolicy.SC_ugetrlimit:
            self.sc_table[sc] = self._CONT
        pass

    pass


class KillerPolicy(MinimalPolicy):

    def __init__(self, sbox, signo):
        super(KillerPolicy, self).__init__(sbox)
        self.signo = signo
        self.job = None
        pass

    def __call__(self, e, a):

        def job(pid, signo):
            if pid and signo:
                os.kill(pid, signo)
            pass

        if self.job is None:
            self.job = threading.Thread(target=job, args=(self.sbox.pid, self.signo))
            self.job.start()

        return super(KillerPolicy, self).__call__(e, a)

    pass


class SelectiveOpenPolicy(MinimalPolicy):

    SC_open = ((2, 0), (5, 1), ) if machine() == 'x86_64' else (5, )
    SC_close = ((3, 0), (6, 1), ) if machine() == 'x86_64' else (6, )

    def __init__(self, sbox, acl):
        super(SelectiveOpenPolicy, self).__init__(sbox)
        for sc in SelectiveOpenPolicy.SC_open:
            self.sc_table[sc] = self.SYS_open
        for sc in SelectiveOpenPolicy.SC_close:
            self.sc_table[sc] = self.SYS_close
        self.acl = set(acl)
        self.fdsafe = set([0, 1, 2])
        pass

    def SYS_open(self, e, a):
        if e.type == S_EVENT_SYSCALL:
            path, mode = self.sbox.dump(T_STRING, e.ext1), e.ext2
            if path is None:  # bad address causing dump failure
                return self._KILL_RT(e, a)
            if not (path, mode) in self.acl:
                return self._KILL_RF(e, a)
        else:
            if e.ext1 >= 0:
                self.fdsafe.add(e.ext1)
        return self._CONT(e, a)

    def SYS_close(self, e, a):
        if e.type == S_EVENT_SYSCALL:
            if not e.ext1 in self.fdsafe:
                return self._KILL_RF(e, a)
            self.fdsafe.remove(e.ext1)
        return self._CONT(e, a)

    pass
