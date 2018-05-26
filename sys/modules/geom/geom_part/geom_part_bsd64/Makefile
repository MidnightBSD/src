# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/geom/geom_part/geom_part_bsd64/Makefile 267359 2014-06-11 10:42:34Z ae $

.PATH: ${.CURDIR}/../../../../geom/part

KMOD=	geom_part_bsd64
SRCS=	g_part_bsd64.c

SRCS+=	bus_if.h device_if.h g_part_if.h

MFILES=	kern/bus_if.m kern/device_if.m geom/part/g_part_if.m

.include <bsd.kmod.mk>
