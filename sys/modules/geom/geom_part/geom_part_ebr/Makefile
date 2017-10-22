# $FreeBSD: release/10.0.0/sys/modules/geom/geom_part/geom_part_ebr/Makefile 193123 2009-05-30 22:04:18Z marcel $

.PATH: ${.CURDIR}/../../../../geom/part

KMOD=	geom_part_ebr
SRCS=	g_part_ebr.c

SRCS+=	bus_if.h device_if.h g_part_if.h
SRCS+=	opt_geom.h

MFILES=	kern/bus_if.m kern/device_if.m geom/part/g_part_if.m

.include <bsd.kmod.mk>
