# $FreeBSD: release/7.0.0/sys/modules/wb/Makefile 150636 2005-09-27 18:10:43Z mlaier $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
