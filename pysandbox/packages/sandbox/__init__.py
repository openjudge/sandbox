################################################################################
# The Sandbox Libraries (Python) Package Initializer                           #
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
"""The Sandbox Libraries (Python)

The sandbox libraries (libsandbox & pysandbox) provide API's in C/C++ and Python
for executing and profiling simple (single process) programs in a restricted
environment, or sandbox. These API's can help developers to build automated
profiling tools and watchdogs that capture and block the runtime behaviours of
binary programs according to configurable / programmable policies.

The sandbox libraries are distributed under the terms of the New BSD license
please refer to the plain text file named COPYING in individual packages.

Project Homepage: http://openjudge.net/~liuyu/Project/LibSandbox

GETTING STARTED

  $ python
  >>> from sandbox import *
  >>> s = Sandbox(["/foo/bar.exe", "arg1", "arg2"])
  >>> s.run()
  ...
  >>> s.probe()
  ...
"""

from . import _sandbox
from ._sandbox import SandboxEvent, SandboxAction, SandboxPolicy
from ._sandbox import __version__, __author__


class Sandbox(_sandbox.Sandbox):

    def __init__(self, *args, **kwds):
        # Since 0.3.4-3, sandbox initialization is completed within the
        # __new__() method rather than in __init__(). And initialized sandbox
        # objects expose new *policy* attributes to support policy assignment.
        # While this ensures the atomicity of sandbox initialization, it also
        # breaks backward compatibility with applications that subclass Sandbox
        # and monkey-patch the *policy* argument in down-stream __init__()
        # methods. The following code assumes the old-style *policy patching*,
        # and emulates it with the new-style *policy assignment*.
        super(Sandbox, self).__init__()
        if 'policy' in kwds:
            if isinstance(kwds['policy'], SandboxPolicy):
                self.policy = kwds['policy']
        pass

    def run(self):
        """Execute the sandboxed program. This method blocks the calling
program until the sandboxed program is finished (or terminated).
"""
        return super(Sandbox, self).run()

    def dump(self, typeid, address):
        """Copy the memory block starting from the specificed address of
the sandboxed program's memory space. On success, return an object
built from the obtained data. Possble typeid's and corresponding
return types are as follows,

  - T_CHAR, T_BYTE, T_UBYTE: int
  - T_SHORT, T_USHORT, T_INT, T_UINT, T_LONG, T_ULONG: int
  - T_FLOAT, T_DOUBLE: float
  - T_STRING: str

On failure, return None in case the specified address is invalid
for dump, otherwise raise an exception.
"""
        try:
            return super(Sandbox, self).dump(typeid, address)
        except ValueError:
            return None
        pass

    def probe(self, compatible=True):
        """Return a dictionary containing runtime statistics of the sandboxed
program. By default, the result contains the following entries,

  - cpu_info (4-tuple):
      0 (int): cpu clock time usage (msec)
      1 (int): cpu time usage in user mode (msec)
      2 (int): cpu time usage in kernel mode (msec)
      3 (long): time-stamp counter (# of instructions)
  - mem_info (6-tuple):
      0 (int): runtime virtual memory usage (kilobytes)
      1 (int): peak virtual memory usage (kilobytes)
      2 (int): runtime resident set size (kilobytes)
      3 (int): peak resident set size (kilobytes)
      4 (int): minor page faults (# of pages)
      5 (int): major page faults (# of pages)
  - signal_info (2-tuple):
      0 (int): last / current signal number
      1 (int): last / current signal code
  - syscall_info (2-tuple):
      0 (int): last / current system call number
      1 (int): last / current system call mode
  - elapsed (int): elapsed wallclock time since started (msec)
  - exitcode (int): exit status of the sandboxed program

When the optional argument *compatible* is True, the result
additionally contains the following entries,

  - cpu (int): cpu time usage (msec)
  - cpu.usr (int): cpu time usage in user mode (msec)
  - cpu.sys (int): cpu time usage in kernel mode (msec)
  - cpu.tsc (long): time-stamp counter (# of instructions)
  - mem.vsize (int): peak virtual memory usage (kilobytes)
  - mem.rss (int): peak resident set size (kilobytes)
  - mem.minflt (int): minor page faults (# of pages)
  - mem.majflt (int): major page faults (# of pages)
  - signal (int): last / current signal number
  - syscall (int): last / current system call number
"""
        data = super(Sandbox, self).probe()
        if data and compatible:
            # cpu_info
            ctime, utime, stime, tsc = data['cpu_info'][:4]
            data['cpu'] = ctime
            data['cpu.usr'] = utime
            data['cpu.sys'] = stime
            data['cpu.tsc'] = tsc
            # mem_info
            vm, vmpeak, rss, rsspeak, minflt, majflt = data['mem_info'][:6]
            data['mem.vsize'] = vmpeak
            data['mem.rss'] = rsspeak
            data['mem.minflt'] = minflt
            data['mem.majflt'] = majflt
            # signal_info
            signo, code = data['signal_info']
            data['signal'] = signo
            # syscall_info
            scno, mode = data['syscall_info']
            data['syscall'] = scno
        return data
    pass


# sandbox event types
S_EVENT_ERROR = SandboxEvent.S_EVENT_ERROR
S_EVENT_EXIT = SandboxEvent.S_EVENT_EXIT
S_EVENT_SIGNAL = SandboxEvent.S_EVENT_SIGNAL
S_EVENT_SYSCALL = SandboxEvent.S_EVENT_SYSCALL
S_EVENT_SYSRET = SandboxEvent.S_EVENT_SYSRET
S_EVENT_QUOTA = SandboxEvent.S_EVENT_QUOTA

# sandbox action types
S_ACTION_CONT = SandboxAction.S_ACTION_CONT
S_ACTION_KILL = SandboxAction.S_ACTION_KILL
S_ACTION_FINI = SandboxAction.S_ACTION_FINI

# sandbox quota types
S_QUOTA_WALLCLOCK = Sandbox.S_QUOTA_WALLCLOCK
S_QUOTA_CPU = Sandbox.S_QUOTA_CPU
S_QUOTA_MEMORY = Sandbox.S_QUOTA_MEMORY
S_QUOTA_DISK = Sandbox.S_QUOTA_DISK

# sandbox status
S_STATUS_PRE = Sandbox.S_STATUS_PRE
S_STATUS_RDY = Sandbox.S_STATUS_RDY
S_STATUS_EXE = Sandbox.S_STATUS_EXE
S_STATUS_BLK = Sandbox.S_STATUS_BLK
S_STATUS_FIN = Sandbox.S_STATUS_FIN

# sandbox native results
S_RESULT_PD = Sandbox.S_RESULT_PD
S_RESULT_OK = Sandbox.S_RESULT_OK
S_RESULT_RF = Sandbox.S_RESULT_RF
S_RESULT_ML = Sandbox.S_RESULT_ML
S_RESULT_OL = Sandbox.S_RESULT_OL
S_RESULT_TL = Sandbox.S_RESULT_TL
S_RESULT_RT = Sandbox.S_RESULT_RT
S_RESULT_AT = Sandbox.S_RESULT_AT
S_RESULT_IE = Sandbox.S_RESULT_IE
S_RESULT_BP = Sandbox.S_RESULT_BP
S_RESULT_R0 = Sandbox.S_RESULT_R0
S_RESULT_R1 = Sandbox.S_RESULT_R1
S_RESULT_R2 = Sandbox.S_RESULT_R2
S_RESULT_R3 = Sandbox.S_RESULT_R3
S_RESULT_R4 = Sandbox.S_RESULT_R4
S_RESULT_R5 = Sandbox.S_RESULT_R5

# datatype indicators
T_BYTE = Sandbox.T_BYTE
T_SHORT = Sandbox.T_SHORT
T_INT = Sandbox.T_INT
T_LONG = Sandbox.T_LONG
T_UBYTE = Sandbox.T_UBYTE
T_USHORT = Sandbox.T_USHORT
T_UINT = Sandbox.T_UINT
T_ULONG = Sandbox.T_ULONG
T_FLOAT = Sandbox.T_FLOAT
T_DOUBLE = Sandbox.T_DOUBLE
T_CHAR = Sandbox.T_CHAR
T_STRING = Sandbox.T_STRING
