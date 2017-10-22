# $FreeBSD: release/10.0.0/sys/modules/sound/driver/mss/Makefile 106497 2002-11-06 13:47:00Z nyan $

.PATH: ${.CURDIR}/../../../../dev/sound/isa

KMOD=	snd_mss
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	mss.c gusc.c

.include <bsd.kmod.mk>
