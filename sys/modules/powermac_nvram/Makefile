# $FreeBSD: stable/11/sys/modules/powermac_nvram/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/powermac_nvram

KMOD=	powermac_nvram
SRCS=	powermac_nvram.c powermac_nvramvar.h
SRCS+=	bus_if.h device_if.h
SRCS+=	ofw_bus_if.h

.include <bsd.kmod.mk>
