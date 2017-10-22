# $FreeBSD: release/10.0.0/usr.sbin/dconschat/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

WARNS?=	1

.include <bsd.prog.mk>
