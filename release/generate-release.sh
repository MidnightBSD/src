#!/bin/sh

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
# $MidnightBSD$
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

