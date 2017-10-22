#	@(#)Makefile	8.1 (Berkeley) 6/7/93
# $FreeBSD: release/10.0.0/usr.bin/mklocale/Makefile 249657 2013-04-19 19:45:00Z ed $

PROG=	mklocale
SRCS=	yacc.y lex.l y.tab.h
CFLAGS+= -I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale

NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.prog.mk>
