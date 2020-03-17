# $FreeBSD: stable/11/sys/modules/sound/driver/sb16/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sound/isa

KMOD=	snd_sb16
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sb16.c

.include <bsd.kmod.mk>
