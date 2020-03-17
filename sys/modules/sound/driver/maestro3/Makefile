# $FreeBSD: stable/11/sys/modules/sound/driver/maestro3/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sound/pci

KMOD=	snd_maestro3
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro3.c

CFLAGS+= -Wall -DM3_DEBUG_LEVEL=-1

.include <bsd.kmod.mk>
