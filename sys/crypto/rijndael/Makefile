# $MidnightBSD$
# $FreeBSD: stable/10/sys/crypto/rijndael/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=test00
MAN=
SRCS= ${PROG}.c rijndael-alg-fst.c rijndael-api-fst.c

CFLAGS += -I${.CURDIR}/../.. -g -static

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG}
