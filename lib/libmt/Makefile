# $MidnightBSD$
# $FreeBSD: stable/10/lib/libmt/Makefile 279219 2015-02-23 21:59:30Z ken $

LIB=		mt
SHLIBDIR?=	/lib
SRCS=		mtlib.c
INCS=		mtlib.h

DPADD=		${LIBSBUF}
LDADD=		-lsbuf

MAN=		mt.3

.include <bsd.lib.mk>
