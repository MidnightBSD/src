# $FreeBSD: release/10.0.0/sys/modules/geom/geom_part/geom_part_ldm/Makefile 233177 2012-03-19 13:15:48Z ae $

.PATH: ${.CURDIR}/../../../../geom/part

KMOD=	geom_part_ldm
SRCS=	g_part_ldm.c

SRCS+=	bus_if.h device_if.h g_part_if.h

MFILES=	kern/bus_if.m kern/device_if.m geom/part/g_part_if.m

.include <bsd.kmod.mk>
