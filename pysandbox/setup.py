#!/usr/bin/env python
################################################################################
# The Sandbox Libraries (Python) Distutil Script                               #
#                                                                              #
# Copyright (C) 2004-2009, 2011 LIU Yu, pineapple.liu@gmail.com                #
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

from distutils.core import setup, Extension
from glob import glob
from os.path import join, normpath

NAME = 'pysandbox'
VERSION = "0.3.3"
RELEASE = "rc3"
AUTHOR = "LIU Yu"
AUTHOR_EMAIL = "pineapple.liu@gmail.com"
MAINTAINER = AUTHOR
MAINTAINER_EMAIL = AUTHOR_EMAIL
URL = "http://sourceforge.net/projects/libsandbox"
LICENSE = "BSD License"
DESCRIPTION = "The Sandbox Libraries (Python)"
LONG_DESCRIPTION = \
"""The sandbox libraries (libsandbox & pysandbox) provide API's in C/C++/Python 
for executing simple (single process) programs in a restricted environment, or
sandbox. Runtime behaviours of binary executable programs can be captured and 
blocked according to configurable / programmable policies."""

setup(name=NAME, 
      version=VERSION, 
      description=DESCRIPTION, 
      long_description=LONG_DESCRIPTION, 
      author=AUTHOR, 
      author_email=AUTHOR_EMAIL, 
      maintainer=MAINTAINER, 
      maintainer_email=MAINTAINER_EMAIL, 
      license=LICENSE, 
      url=URL,
      packages=['sandbox'], 
      ext_package='sandbox',
      ext_modules=[Extension('_sandbox', 
                             language='c',
                             define_macros=[('SANDBOX', None), 
                                            ('NDEBUG', None), 
                                            ('AUTHOR', '"%s <%s>"' % \
                                             (AUTHOR, AUTHOR_EMAIL)), 
                                            ('VERSION', '"%s-%s"' % \
                                             (VERSION, RELEASE))],
                             undef_macros=['DEBUG'], 
                             extra_compile_args=['-Wall', '-g0', '-O3'], 
                             include_dirs=['sandbox'], 
                             libraries=['sandbox', 'rt'], 
                             sources=glob(join('sandbox', '*.c'))), ])
