# $FreeBSD: release/10.0.0/lib/libsbuf/Makefile 222033 2011-05-17 17:37:58Z phk $

LIB=		sbuf
SHLIBDIR?= 	/lib
SRCS=		subr_sbuf.c 

SHLIB_MAJOR	= 6

SYMBOL_MAPS=    ${.CURDIR}/Symbol.map
VERSION_DEF=	${.CURDIR}/Version.def

.PATH:	${.CURDIR}/../../sys/kern

.include <bsd.lib.mk>
