#	@(#)Makefile	8.1 (Berkeley) 6/7/93
# $FreeBSD: release/7.0.0/usr.bin/mklocale/Makefile 146261 2005-05-16 09:32:41Z ru $

PROG=	mklocale
WARNS?=	6
SRCS=	yacc.y lex.l y.tab.h
CFLAGS+= -I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale

.include <bsd.prog.mk>
