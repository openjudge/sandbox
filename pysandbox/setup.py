#!/usr/bin/env python
################################################################################
# The Sandbox Libraries (Python) Distutil Script                               #
#                                                                              #
# Copyright (C) 2004-2009, 2011, 2012 LIU Yu, pineapple.liu@gmail.com          #
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
"""The sandbox libraries (libsandbox & pysandbox) provide API's in C/C++ and Python
for executing and profiling simple (single process) programs in a restricted 
environment, or sandbox. These API's can help developers to build automated 
profiling tools and watchdogs that capture and block the runtime behaviours of 
binary programs according to configurable / programmable policies."""

from distutils.core import setup, Extension
from glob import glob
from os.path import join, basename

NAME = 'pysandbox'
VERSION = "0.3.5"
RELEASE = "1"
AUTHOR = "LIU Yu"
AUTHOR_EMAIL = "pineapple.liu@gmail.com"
MAINTAINER = AUTHOR
MAINTAINER_EMAIL = AUTHOR_EMAIL
URL = "http://sourceforge.net/projects/libsandbox"
LICENSE = "BSD License"
DESCRIPTION = "The Sandbox Libraries (Python)"

try:
    # python 2.7+
    from subprocess import check_output
except ImportError:
    # python 2.6
    from commands import getoutput
    check_output = lambda tup: getoutput(' '.join(tup))

try:
    pkgconfig = ['pkg-config', '--silence-errors', 'libsandbox']
    core_ccflags = check_output(pkgconfig + ['--cflags', ]).decode().split()
    core_ldflags = check_output(pkgconfig + ['--libs', ]).decode().split()
except:
    core_ccflags = ['-pthread', ]
    core_ldflags = ['-lsandbox', '-lrt']

_sandbox = Extension('_sandbox',
    language='c',
    define_macros=[('SANDBOX', None), 
                   ('NDEBUG', None), 
                   ('AUTHOR', '"%s <%s>"' % \
                   (AUTHOR, AUTHOR_EMAIL)), 
                   ('VERSION', '"%s-%s"' % \
                   (VERSION, RELEASE))],
    undef_macros=['DEBUG'], 
    extra_compile_args=['-Wall', '-g0', '-O3', '-Wno-write-strings'] + core_ccflags, 
    extra_link_args=[] + core_ldflags, 
    include_dirs=[join('packages', 'sandbox'), ], 
    sources=glob(join('packages', 'sandbox', '*.c')))

setup(name=NAME, 
      version=VERSION, 
      description=DESCRIPTION, 
      long_description=__doc__, 
      author=AUTHOR, 
      author_email=AUTHOR_EMAIL, 
      maintainer=MAINTAINER, 
      maintainer_email=MAINTAINER_EMAIL, 
      license=LICENSE, 
      url=URL, 
      package_dir = {'sandbox': join('packages', 'sandbox'), },
      packages=['sandbox', ], 
      ext_package='sandbox',
      ext_modules=[_sandbox, ])
