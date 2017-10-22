# $FreeBSD: release/10.0.0/sys/modules/vr/Makefile 177048 2008-03-11 03:50:57Z yongari $

.PATH: ${.CURDIR}/../../dev/vr

KMOD=	if_vr
SRCS=	if_vr.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
