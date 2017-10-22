# $FreeBSD: release/10.0.0/sys/modules/dc/Makefile 151438 2005-10-18 06:11:08Z imp $

.PATH: ${.CURDIR}/../../dev/dc

KMOD=	if_dc
SRCS=	if_dc.c dcphy.c pnphy.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
