#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: release/7.0.0/sbin/dmesg/Makefile 140383 2005-01-17 13:56:46Z delphij $

PROG=	dmesg
MAN=	dmesg.8

WARNS?=	6

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
