# $FreeBSD: release/10.0.0/sys/modules/sis/Makefile 181524 2008-08-10 10:00:14Z imp $

.PATH: ${.CURDIR}/../../dev/sis

KMOD=	if_sis
SRCS=	if_sis.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
