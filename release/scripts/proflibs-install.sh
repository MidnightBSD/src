#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-install.sh,v 1.4 2001/04/08 23:09:21 obrien Exp $
# $MidnightBSD: src/release/scripts/proflibs-install.sh,v 1.2 2007/03/17 16:44:45 laffer1 Exp $

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat proflibs.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
