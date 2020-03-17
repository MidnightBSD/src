# $FreeBSD: stable/11/sys/modules/streams/Makefile 298519 2016-04-23 20:29:55Z dchagin $ 

.PATH: ${.CURDIR}/../../dev/streams

KMOD=	streams
SRCS=	streams.c

.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif
CFLAGS+= -O

.include <bsd.kmod.mk>
