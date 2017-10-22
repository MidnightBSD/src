#	@(#)Makefile	8.1 (Berkeley) 6/7/93
# $FreeBSD: stable/9/usr.bin/mklocale/Makefile 201386 2010-01-02 10:27:05Z ed $

PROG=	mklocale
SRCS=	yacc.y lex.l y.tab.h
CFLAGS+= -I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale

.include <bsd.prog.mk>
