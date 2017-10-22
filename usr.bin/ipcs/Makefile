# $FreeBSD: release/10.0.0/usr.bin/ipcs/Makefile 201386 2010-01-02 10:27:05Z ed $

PROG=	ipcs
SRCS=	ipcs.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm

WARNS?=	1

.include <bsd.prog.mk>
