# $FreeBSD: stable/11/sys/modules/imx/imx_spi/Makefile 331519 2018-03-25 01:47:17Z ian $

.PATH: ${SRCTOP}/sys/arm/freescale/imx

KMOD=	imx_spi
SRCS=	imx_spi.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	gpio_if.h \
	ofw_bus_if.h \
	opt_platform.h \
	spibus_if.h \

.include <bsd.kmod.mk>
