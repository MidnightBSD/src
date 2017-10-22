# $FreeBSD: release/10.0.0/sys/modules/sound/driver/maestro3/Makefile 230898 2012-02-02 00:10:20Z pfg $

.PATH: ${.CURDIR}/../../../../dev/sound/pci

KMOD=	snd_maestro3
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	maestro3.c

CFLAGS+= -Wall -DM3_DEBUG_LEVEL=-1

.include <bsd.kmod.mk>
