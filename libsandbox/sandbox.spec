################################################################################
# The Sandbox Libraries RPM Specification                                      #
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
%define name libsandbox
%define version 0.3.4
%define release 1

Summary: The Sandbox Libraries
Name: %{name}
Version: %{version}
Release: %{release}
Source0: %{name}-%{version}.tar.gz
License: BSD License
Group: Development/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix: %{_prefix}
Vendor: OpenJudge Alliance, http://openjudge.net
Packager: LIU Yu <pineapple.liu@gmail.com>
Url: http://sourceforge.net/projects/libsandbox

Provides: sandbox.h sandbox-dev.h
%ifarch x86_64
Provides: libsandbox.so()(64bit)
%else
Provides: libsandbox.so
%endif

%description
The sandbox libraries (libsandbox & pysandbox) provide API's in C/C++/Python 
for executing simple (single process) programs in a restricted environment, or
sandbox. Runtime behaviours of binary executable programs can be captured and 
blocked according to configurable / programmable policies.


%prep
%setup
./configure --prefix=%{buildroot}%{_prefix} --libdir=%{buildroot}%{_libdir} --includedir=%{buildroot}%{_includedir} --enable-chkvsc

%build
make

%install
make prefix=%{buildroot}%{_prefix} install

%clean
rm -rf %{buildroot}%{_prefix}
rmdir --ignore-fail-on-non-empty %{buildroot}

%files
%defattr(-,root,root)
%{_includedir}/sandbox.h
%{_includedir}/sandbox-dev.h
%{_libdir}/%{name}.so.%{version}
%{_libdir}/%{name}.so
%{_libdir}/%{name}.a
%doc README COPYING CHANGES
