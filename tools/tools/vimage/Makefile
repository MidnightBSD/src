# $FreeBSD: release/10.0.0/tools/tools/vimage/Makefile 196409 2009-08-20 22:39:20Z zec $

PROG=	vimage
LDADD=	-ljail
DPADD=	${LIBJAIL}

WARNS?=	2
CFLAGS+= -I../../../sys

MAN=	vimage.8

BINDIR?=	/usr/sbin

.include <bsd.prog.mk>
