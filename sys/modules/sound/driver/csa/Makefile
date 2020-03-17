# $FreeBSD: stable/11/sys/modules/sound/driver/csa/Makefile 347113 2019-05-04 12:19:02Z avatar $

.PATH: ${SRCTOP}/sys/dev/sound/pci

KMOD=	snd_csa
SRCS=	device_if.h bus_if.h mpufoi_if.h pci_if.h
SRCS+=	csa.c csamidi.c csapcm.c

.include <bsd.kmod.mk>
