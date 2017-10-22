# $FreeBSD: release/7.0.0/sys/modules/vr/Makefile 150636 2005-09-27 18:10:43Z mlaier $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_vr
SRCS=	if_vr.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
