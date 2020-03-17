# $FreeBSD: stable/11/sys/modules/malo/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/malo

KMOD    = if_malo
SRCS    = if_malo.c if_malohal.c if_malo_pci.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_malo.h

opt_malo.h:
	echo '#define MALO_DEBUG 1'> $@

.include <bsd.kmod.mk>
