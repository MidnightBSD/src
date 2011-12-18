# $MidnightBSD$
# $FreeBSD: src/usr.sbin/tzsetup/Makefile,v 1.19 2011/01/12 14:55:02 nwhitehorn Exp $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}/../../gnu/lib/libodialog -I.

WARNS?=	3

DPADD=	${LIBODIALOG} ${LIBNCURSES}
LDADD=	-lodialog -lncurses

.include <bsd.prog.mk>
