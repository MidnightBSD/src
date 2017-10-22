# $FreeBSD: stable/9/tools/test/ppsapi/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0
