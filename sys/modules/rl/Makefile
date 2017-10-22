# $FreeBSD: release/10.0.0/sys/modules/rl/Makefile 184559 2008-11-02 16:50:57Z imp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_rl
SRCS=	if_rl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
