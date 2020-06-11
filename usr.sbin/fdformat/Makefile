# $FreeBSD: stable/11/usr.sbin/fdformat/Makefile 326918 2017-12-17 06:00:49Z cy $

.PATH: ${.CURDIR:H}/fdread

PROG=	fdformat
MAN=	fdformat.8
SRCS=	fdformat.c fdutil.c

CFLAGS+= -I${.CURDIR:H}/fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
