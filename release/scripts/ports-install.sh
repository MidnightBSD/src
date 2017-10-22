#!/bin/sh
#
# $FreeBSD: stable/9/release/scripts/ports-install.sh 209332 2010-06-19 09:33:11Z brian $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting ports tarball into ${DESTDIR}/usr"
tar --unlink -xpzf ports.tgz -C ${DESTDIR}/usr
exit 0
