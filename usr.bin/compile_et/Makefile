# $FreeBSD: stable/9/usr.bin/compile_et/Makefile 201386 2010-01-02 10:27:05Z ed $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

WARNS?=	0

.include <bsd.prog.mk>
