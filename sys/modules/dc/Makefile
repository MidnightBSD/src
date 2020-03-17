# $FreeBSD: stable/11/sys/modules/dc/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/dc

KMOD=	if_dc
SRCS=	if_dc.c dcphy.c pnphy.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
