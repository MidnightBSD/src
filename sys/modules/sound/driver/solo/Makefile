# $FreeBSD: stable/11/sys/modules/sound/driver/solo/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sound/pci

KMOD=	snd_solo
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	solo.c

.include <bsd.kmod.mk>
