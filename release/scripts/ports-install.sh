#!/bin/sh
#
# $FreeBSD$
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting ports tarball into ${DESTDIR}/usr"
tar --unlink -xpzf ports.tgz -C ${DESTDIR}/usr
exit 0
