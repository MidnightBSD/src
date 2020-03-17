# $FreeBSD: stable/11/sys/modules/bhnd/cores/bhnd_chipc/Makefile 300548 2016-05-24 01:12:19Z adrian $

.PATH: ${.CURDIR}/../../../../dev/bhnd/cores/chipc

KMOD=	bhnd_chipc
SRCS=	chipc.c chipc_subr.c \
	bhnd_sprom_chipc.c
SRCS+=	device_if.h bus_if.h bhnd_bus_if.h \
	bhnd_chipc_if.h bhnd_nvram_if.h

.include <bsd.kmod.mk>
