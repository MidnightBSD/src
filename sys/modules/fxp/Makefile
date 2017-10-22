# $FreeBSD: release/10.0.0/sys/modules/fxp/Makefile 226154 2011-10-08 12:33:10Z marius $

.PATH: ${.CURDIR}/../../dev/fxp

KMOD=	if_fxp
SRCS=	device_if.h bus_if.h if_fxp.c inphy.c miibus_if.h miidevs.h pci_if.h

.include <bsd.kmod.mk>
