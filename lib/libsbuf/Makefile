# $MidnightBSD$
# $FreeBSD: src/lib/libsbuf/Makefile,v 1.5 2003/08/18 15:25:39 obrien Exp $

LIB=		sbuf
SHLIBDIR?= 	/lib
SRCS=		subr_sbuf.c 

WARNS?= 2

SHLIB_MAJOR	= 6

SYMBOL_MAPS=    ${.CURDIR}/Symbol.map
VERSION_DEF=	${.CURDIR}/Version.def

.PATH:	${.CURDIR}/../../sys/kern

.include <bsd.lib.mk>
