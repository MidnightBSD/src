#!/bin/sh
+#-
+# Copyright (c) 2011 Nathan Whitehorn
+# All rights reserved.
+#
+# Redistribution and use in source and binary forms, with or without
+# modification, are permitted provided that the following conditions
+# are met:
+# 1. Redistributions of source code must retain the above copyright
+#    notice, this list of conditions and the following disclaimer.
+# 2. Redistributions in binary form must reproduce the above copyright
+#    notice, this list of conditions and the following disclaimer in the
+#    documentation and/or other materials provided with the distribution.
+#
+# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
+# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
+# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
+# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
+# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
+# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
+# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
+# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
+# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
+# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
+# SUCH DAMAGE.
+#

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh cvs-branch scratch-dir
#
# Environment variables:
#  CVSROOT:    CVS root to obtain the mports tree.
#  CVS_TAG:    CVS tag for mports (HEAD by default)
#  MAKE_FLAGS: optional flags to pass to make (e.g. -j)
#  RELSTRING:  optional base name for media images (e.g. MidnightBSD-0.4-RC2-amd64)
# 
#  Note: Since this requires a chroot, release cross-builds will not work!
#
# $MidnightBSD: src/release/generate-release.sh,v 1.1 2011/12/02 13:15:35 laffer1 Exp $
# $FreeBSD: src/release/generate-release.sh,v 1.14 2011/11/15 18:49:27 nwhitehorn Exp $
#

mkdir -p $2/usr/src
set -e # Everything must succeed

if [ ! -z $CVSROOT ]; then
	cd $2/usr
	cvs -R ${CVSARGS} -d ${CVSROOT} co -P -r ${CVS_TAG:-HEAD} mports
	cvs -R ${CVSARGS} -d ${CVSROOT} co -P -r $1 src
fi

cd $2/usr/src
make $MAKE_FLAGS buildworld
make installworld distribution DESTDIR=$2
mount -t devfs devfs $2/dev
trap "umount $2/dev" EXIT # Clean up devfs mount on exit

chroot $2 make -C /usr/src $MAKE_FLAGS buildworld buildkernel
chroot $2 make -C /usr/src/release release
chroot $2 make -C /usr/src/release install DESTDIR=/R

: ${RELSTRING=`chroot $2 uname -s`-`chroot $2 uname -r`-`chroot $2 uname -p`}

cd $2/R
for i in release.iso bootonly.iso memstick; do
	mv $i $RELSTRING-$i
done
sha256 $RELSTRING-* > CHECKSUM.SHA256
md5 $RELSTRING-* > CHECKSUM.MD5

