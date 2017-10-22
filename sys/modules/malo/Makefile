# $FreeBSD: release/10.0.0/sys/modules/malo/Makefile 178354 2008-04-20 20:35:46Z sam $

.PATH: ${.CURDIR}/../../dev/malo

KMOD    = if_malo
SRCS    = if_malo.c if_malohal.c if_malo_pci.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_malo.h

opt_malo.h:
	echo '#define MALO_DEBUG 1'> $@

.include <bsd.kmod.mk>
