#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/9/usr.sbin/iostat/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	iostat
MAN=	iostat.8

DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBM}
LDADD=	-ldevstat -lkvm -lm

WARNS?=	1

.include <bsd.prog.mk>
