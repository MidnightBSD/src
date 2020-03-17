# $FreeBSD: stable/11/sys/modules/fxp/Makefile 345186 2019-03-15 15:16:31Z markj $

.PATH: ${SRCTOP}/sys/dev/fxp

KMOD=	if_fxp
SRCS=	device_if.h bus_if.h if_fxp.c inphy.c miibus_if.h miidevs.h pci_if.h

.include <bsd.kmod.mk>
