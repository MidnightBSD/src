# $FreeBSD: stable/11/sbin/ggate/ggated/Makefile 319185 2017-05-30 04:17:22Z ngie $

.PATH: ${.CURDIR:H}/shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

LIBADD=	pthread util

CFLAGS+= -I${.CURDIR:H}/shared

.include <bsd.prog.mk>
