# $FreeBSD: stable/11/sys/modules/imx/imx6_ahci/Makefile 336079 2018-07-08 00:27:28Z ian $

.PATH: ${SRCTOP}/sys/arm/freescale/imx

KMOD=	imx6_ahci
SRCS=	imx6_ahci.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	ofw_bus_if.h \

.include <bsd.kmod.mk>
