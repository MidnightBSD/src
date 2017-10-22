# $FreeBSD: release/7.0.0/gnu/usr.bin/dc/Makefile 125910 2004-02-17 01:34:53Z ache $

BCDIR=	${.CURDIR}/../../../contrib/bc

.PATH:	${BCDIR}/dc ${BCDIR}/lib ${BCDIR}/doc

PROG=	dc
SRCS=	array.c dc.c eval.c misc.c numeric.c stack.c string.c \
	number.c
CFLAGS+=-I${.CURDIR}/../bc -I${BCDIR}/h -DHAVE_CONFIG_H
DPADD=	${LIBM}
LDADD=	-lm
SUBDIR=	doc

.include <bsd.prog.mk>
