# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.sbin/iostat/Makefile 296995 2016-03-17 20:29:10Z asomers $

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBM}
LDADD=	-ldevstat -lkvm -lm

.include <bsd.prog.mk>
