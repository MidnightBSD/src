# $FreeBSD: stable/11/usr.bin/chkey/Makefile 319186 2017-05-30 04:27:05Z ngie $

.include <src.opts.mk>

.PATH: ${SRCTOP}/usr.bin/newkey

PROG=	chkey
SRCS=	chkey.c generic.c update.c
CFLAGS+= -I${SRCTOP}/usr.bin/newkey
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.endif
LIBADD+=	rpcsvc mp

.include <bsd.prog.mk>
