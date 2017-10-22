# $FreeBSD: release/10.0.0/sys/modules/tl/Makefile 181738 2008-08-14 20:02:34Z imp $

.PATH: ${.CURDIR}/../../dev/tl

KMOD=	if_tl
SRCS=	if_tl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
