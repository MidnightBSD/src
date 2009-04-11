#!/bin/mksh
#
# $MidnightBSD$
#
# Copyright (C) 2009
#         Lucas Holt. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# usage:
# makeman.sh /usr/share/man/ outputdir

DIR=$1
OUT=$2
MANPATHS="man1 man2 man3 man4 man5 man6 man7 man8 man9"

if [ -d "$DIR" ]; then
	echo "Using $DIR for man files"
else
	echo "Bad directory"
	exit 1
fi

if [ -d "$OUT" ]; then
	echo "Using $OUT for output"
else
	echo "Bad output directory"
	exit 1
fi

for manpath in $MANPATHS; do
	if [ -d "$DIR/$manpath" ]; then
		for file in $DIR/$manpath/*.gz ; do
			echo "$file"
			MYNAME=`basename  "$file" .gz`
			if [ -f "$file" ]; then
				gunzip -q -c "$file" | groff -Thtml -m man  > "$OUT/$MYNAME".html
			fi
		done
	else
		echo "$DIR/$manpath does not exist"
	fi
done
