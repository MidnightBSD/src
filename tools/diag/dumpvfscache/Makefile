# $FreeBSD: release/10.0.0/tools/diag/dumpvfscache/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>
