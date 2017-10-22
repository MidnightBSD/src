# $FreeBSD: release/10.0.0/sys/modules/geom/geom_part/geom_part_vtoc8/Makefile 177512 2008-03-23 01:42:47Z marcel $

.PATH: ${.CURDIR}/../../../../geom/part

KMOD=	geom_part_vtoc8
SRCS=	g_part_vtoc8.c

SRCS+=	bus_if.h device_if.h g_part_if.h

MFILES=	kern/bus_if.m kern/device_if.m geom/part/g_part_if.m

.include <bsd.kmod.mk>
