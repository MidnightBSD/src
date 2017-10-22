#!/bin/sh
#
# $FreeBSD: release/7.0.0/release/scripts/ports-install.sh 75328 2001-04-08 23:09:21Z obrien $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting ports tarball into ${DESTDIR}/usr"
cat ports.tgz | tar --unlink -xpzf - -C ${DESTDIR}/usr
exit 0
