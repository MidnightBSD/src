#	@(#)Makefile	8.1 (Berkeley) 6/7/93
# $FreeBSD: stable/11/usr.bin/mklocale/Makefile 319186 2017-05-30 04:27:05Z ngie $

PROG=	mklocale
SRCS=	yacc.y lex.l y.tab.h
CFLAGS+= -I. -I${.CURDIR} -I${SRCTOP}/lib/libc/locale

NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.prog.mk>
