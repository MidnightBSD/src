# $FreeBSD: release/7.0.0/usr.bin/compile_et/Makefile 151328 2005-10-14 15:50:22Z ru $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>
