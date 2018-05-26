# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/streams/Makefile 299215 2016-05-07 08:30:21Z dchagin $ 

.PATH: ${.CURDIR}/../../dev/streams

KMOD=	streams
SRCS=	streams.c

.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif
CFLAGS+= -O

.include <bsd.kmod.mk>
