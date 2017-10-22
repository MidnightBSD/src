# $FreeBSD: release/7.0.0/usr.sbin/kernbb/Makefile 123122 2003-12-03 07:43:03Z phk $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

