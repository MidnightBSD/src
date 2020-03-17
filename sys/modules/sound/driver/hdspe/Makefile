# $FreeBSD: stable/11/sys/modules/sound/driver/hdspe/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sound/pci

KMOD=	snd_hdspe
SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	hdspe.c hdspe-pcm.c hdspe.h

.include <bsd.kmod.mk>
