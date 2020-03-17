# $FreeBSD: stable/11/sys/modules/bhnd/bhndb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/bhnd/bhndb

KMOD=	bhndb
SRCS=	bhndb.c bhndb_subr.c bhndb_hwdata.c \
	bhnd_bhndb.c \
	bhndb_bus_if.c bhndb_bus_if.h \
	bhndb_if.c bhndb_if.h
SRCS+=	bhnd_bus_if.h \
	bhnd_chipc_if.h \
	bhnd_nvram_if.h

SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
