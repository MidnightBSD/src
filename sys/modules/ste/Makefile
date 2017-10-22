# $FreeBSD: release/10.0.0/sys/modules/ste/Makefile 181739 2008-08-14 20:09:58Z imp $

.PATH: ${.CURDIR}/../../dev/ste

KMOD=	if_ste
SRCS=	if_ste.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
