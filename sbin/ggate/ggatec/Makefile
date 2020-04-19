# $FreeBSD: stable/11/sbin/ggate/ggatec/Makefile 319185 2017-05-30 04:17:22Z ngie $

.PATH: ${.CURDIR:H}/shared

PROG=	ggatec
MAN=	ggatec.8
SRCS=	ggatec.c ggate.c

CFLAGS+= -DMAX_SEND_SIZE=32768
CFLAGS+= -DLIBGEOM
CFLAGS+= -I${.CURDIR:H}/shared

LIBADD=	geom util pthread

.include <bsd.prog.mk>
