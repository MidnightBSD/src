#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-install.sh,v 1.1 2006/08/28 08:13:56 ru Exp $
# $MidnightBSD$

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat lib32.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
