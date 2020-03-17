# $FreeBSD: stable/11/sys/modules/imx/imx_wdog/Makefile 336070 2018-07-07 17:25:09Z ian $

.PATH: ${SRCTOP}/sys/arm/freescale/imx

KMOD=	imx_wdog
SRCS=	imx_wdog.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	ofw_bus_if.h \

.include <bsd.kmod.mk>
