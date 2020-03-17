# $FreeBSD: stable/11/sys/modules/fdt/fdt_slicer/Makefile 346557 2019-04-22 15:04:11Z ian $

.PATH: ${SRCTOP}/sys/dev/fdt

KMOD=	fdt_slicer
SRCS=	fdt_slicer.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	ofw_bus_if.h \
	opt_platform.h \

.include <bsd.kmod.mk>
