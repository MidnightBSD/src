# $FreeBSD: stable/11/sys/modules/wb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/wb

KMOD=	if_wb
SRCS=	if_wb.c device_if.h
SRCS+=	bus_if.h miibus_if.h pci_if.h

.include <bsd.kmod.mk>
