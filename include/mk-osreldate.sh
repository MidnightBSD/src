#!/bin/sh -
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2013 Garrett Cooper
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

set -e

CURDIR=$(pwd)
ECHO=${ECHO:=echo}

tmpfile=$(mktemp osreldate.XXXXXXXX)
trap "rm -f $tmpfile" EXIT

${ECHO} creating osreldate.h from newvers.sh

set +e
COPYRIGHT=$(sh ${NEWVERS_SH:=$CURDIR/../sys/conf/newvers.sh} -c) || exit 1
eval $(sh ${NEWVERS_SH} -V RELDATE) || exit 1
set -e
cat > $tmpfile <<EOF
$COPYRIGHT
#ifdef _KERNEL
#error "<osreldate.h> cannot be used in the kernel, use <sys/param.h>"
#else
#undef __MidnightBSD_version
#define __MidnightBSD_version $RELDATE
#endif
EOF
chmod 644 $tmpfile
mv -f $tmpfile osreldate.h
