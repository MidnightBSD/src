# $FreeBSD: stable/11/usr.bin/compile_et/Makefile 319186 2017-05-30 04:27:05Z ngie $

.PATH: ${SRCTOP}/contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l
LIBADD=	roken vers
CFLAGS+=-I. -I${SRCTOP}/contrib/com_err

WARNS?=	0

.include <bsd.prog.mk>
