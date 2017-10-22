#!/bin/sh
#
# $FreeBSD: stable/9/release/scripts/lib32-install.sh 161689 2006-08-28 08:13:56Z ru $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat lib32.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
