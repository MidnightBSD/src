#!/bin/sh
#
# $FreeBSD: stable/9/release/scripts/proflibs-install.sh 161688 2006-08-28 08:12:49Z ru $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat proflibs.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
