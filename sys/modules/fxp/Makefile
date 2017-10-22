# $FreeBSD: stable/9/sys/modules/fxp/Makefile 227229 2011-11-06 17:23:49Z marius $

.PATH: ${.CURDIR}/../../dev/fxp

KMOD=	if_fxp
SRCS=	device_if.h bus_if.h if_fxp.c inphy.c miibus_if.h miidevs.h pci_if.h

.include <bsd.kmod.mk>
