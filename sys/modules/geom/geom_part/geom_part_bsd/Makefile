# $FreeBSD: stable/11/sys/modules/geom/geom_part/geom_part_bsd/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/geom/part ${SRCTOP}/sys/geom

KMOD=	geom_part_bsd
SRCS=	g_part_bsd.c geom_bsd_enc.c

SRCS+=	bus_if.h device_if.h g_part_if.h

.include <bsd.kmod.mk>
