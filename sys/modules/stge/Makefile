# $FreeBSD: release/10.0.0/sys/modules/stge/Makefile 160642 2006-07-25 00:45:55Z yongari $

.PATH: ${.CURDIR}/../../dev/stge

KMOD=	if_stge
SRCS=	if_stge.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
