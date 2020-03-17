# $FreeBSD: stable/11/sys/modules/bhnd/bhndb_pci/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/bhnd/bhndb

KMOD=	bhndb_pci
SRCS=	bhndb_pci.c bhndb_pci_hwdata.c \
	bhndb_pci_sprom.c
SRCS+=	bhnd_bus_if.h bhndb_bus_if.h bhndb_if.h
SRCS+=	bhnd_nvram_if.h

SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
