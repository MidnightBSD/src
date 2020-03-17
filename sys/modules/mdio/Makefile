# $FreeBSD: stable/11/sys/modules/mdio/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mdio

KMOD=	mdio
SRCS=	mdio.c
SRCS+=	mdio_if.c mdio_if.h
SRCS+=	device_if.h bus_if.h

.include <bsd.kmod.mk>
