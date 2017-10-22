# $FreeBSD: release/7.0.0/usr.sbin/dconschat/Makefile 140909 2005-01-27 14:52:47Z delphij $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
