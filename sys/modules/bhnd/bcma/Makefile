# $FreeBSD: stable/11/sys/modules/bhnd/bcma/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/bhnd/bcma

KMOD=	bcma
SRCS=	bcma.c bcma_subr.c bcma_erom.c

SRCS+=	device_if.h bus_if.h bhnd_bus_if.h

.include <bsd.kmod.mk>
