# $FreeBSD: release/10.0.0/sys/modules/geom/geom_part/geom_part_pc98/Makefile 177692 2008-03-28 17:58:55Z marcel $

.PATH: ${.CURDIR}/../../../../geom/part

KMOD=	geom_part_pc98
SRCS=	g_part_pc98.c

SRCS+=	bus_if.h device_if.h g_part_if.h

MFILES=	kern/bus_if.m kern/device_if.m geom/part/g_part_if.m

.include <bsd.kmod.mk>
