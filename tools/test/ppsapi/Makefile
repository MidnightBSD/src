# $FreeBSD: stable/11/tools/test/ppsapi/Makefile 264400 2014-04-13 05:21:56Z imp $

PROG=	ppsapitest
MAN=

WARNS?=	5

.include <bsd.prog.mk>

CFLAGS+=-Wno-format-security

test:	${PROG}
	./${PROG} /dev/cuau0
