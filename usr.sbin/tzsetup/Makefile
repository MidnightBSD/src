# $MidnightBSD: src/usr.sbin/tzsetup/Makefile,v 1.2 2011/12/18 03:45:57 laffer1 Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}/../../gnu/lib/libodialog -I.

WARNS?=	3

DPADD=	${LIBODIALOG} ${LIBNCURSES}
LDADD=	-lodialog -lncurses

.include <bsd.prog.mk>
