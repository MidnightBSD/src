# $FreeBSD: stable/11/lib/libsbuf/Makefile 321114 2017-07-18 08:23:47Z ngie $

PACKAGE=lib${LIB}
LIB=		sbuf
SHLIBDIR?= 	/lib
SRCS=		subr_prf.c subr_sbuf.c

SHLIB_MAJOR	= 6

SYMBOL_MAPS=    ${.CURDIR}/Symbol.map
VERSION_DEF=	${.CURDIR}/Version.def

.include <src.opts.mk>

.PATH:	${SRCTOP}/sys/kern

SUBDIR.${MK_TESTS}+=	tests

.include <bsd.lib.mk>
