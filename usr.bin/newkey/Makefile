# $FreeBSD: stable/11/usr.bin/newkey/Makefile 275042 2014-11-25 14:29:10Z bapt $

.include <src.opts.mk>

PROG=	newkey
SRCS=	newkey.c generic.c update.c
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
MAN=	newkey.8
LIBADD=	rpcsvc mp

.include <bsd.prog.mk>
