#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $MidnightBSD$

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBM}
LDADD=	-ldevstat -lkvm -lm

WARNS?=	1

.include <bsd.prog.mk>
