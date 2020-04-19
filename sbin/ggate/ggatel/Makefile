# $FreeBSD: stable/11/sbin/ggate/ggatel/Makefile 319185 2017-05-30 04:17:22Z ngie $

.PATH: ${.CURDIR:H}/shared

PROG=	ggatel
MAN=	ggatel.8
SRCS=	ggatel.c ggate.c

CFLAGS+= -DLIBGEOM
CFLAGS+= -I${.CURDIR:H}/shared

LIBADD=	geom util

.include <bsd.prog.mk>
