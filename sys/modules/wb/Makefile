# $FreeBSD: release/10.0.0/sys/modules/wb/Makefile 181741 2008-08-14 21:26:29Z imp $

.PATH: ${.CURDIR}/../../dev/wb

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
