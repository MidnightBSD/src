#	$OpenBSD: Makefile,v 1.4 2006/06/30 19:02:28 otto Exp $
#	$MidnightBSD$

PROG=		bc
SRCS=		bc.y scan.l
CFLAGS+=	-I. -I${.CURDIR} -Wall -Wno-unused
YFLAGS+=

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

SUBDIR+= USD.doc

.include <bsd.prog.mk>
