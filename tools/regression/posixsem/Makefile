# $FreeBSD: stable/9/tools/regression/posixsem/Makefile 180059 2008-06-27 05:39:04Z jhb $

PROG=	posixsem
SRCS=	posixsem.c test.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
NO_MAN=

WARNS?=	3

.include <bsd.prog.mk>
