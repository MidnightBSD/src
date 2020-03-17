# $FreeBSD: stable/11/sys/modules/bhnd/bcma_bhndb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/bhnd/bcma

KMOD=	bcma_bhndb
SRCS=	bcma_bhndb.c

SRCS+=	bhnd_bus_if.h bhndb_bus_if.h bhndb_if.h
SRCS+=	device_if.h bus_if.h

.include <bsd.kmod.mk>
