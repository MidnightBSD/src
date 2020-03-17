# $FreeBSD: stable/11/sys/crypto/rijndael/Makefile 264400 2014-04-13 05:21:56Z imp $

PROG=test00
MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
