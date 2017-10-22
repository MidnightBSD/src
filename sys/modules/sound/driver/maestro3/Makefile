# $FreeBSD: stable/9/sys/modules/sound/driver/maestro3/Makefile 110498 2003-02-07 13:56:33Z nyan $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro3
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro3.c

CFLAGS+= -Wall -DM3_DEBUG_LEVEL=-1

.include <bsd.kmod.mk>
