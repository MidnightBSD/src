# $FreeBSD: stable/11/sys/modules/sound/driver/mss/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sound/isa

KMOD=	snd_mss
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	mss.c gusc.c

.include <bsd.kmod.mk>
