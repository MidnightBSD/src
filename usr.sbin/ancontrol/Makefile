# $FreeBSD: release/10.0.0/usr.sbin/ancontrol/Makefile 132901 2004-07-30 21:43:21Z njl $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${.CURDIR}/../../sys

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
