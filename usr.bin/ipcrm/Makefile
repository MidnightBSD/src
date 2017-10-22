# $FreeBSD: release/10.0.0/usr.bin/ipcrm/Makefile 201386 2010-01-02 10:27:05Z ed $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

WARNS?=	0

.include <bsd.prog.mk>
