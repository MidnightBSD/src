# $FreeBSD: stable/11/sys/modules/i2c/icee/Makefile 323691 2017-09-17 22:58:13Z ian $

.PATH:	${SRCTOP}/sys/dev/iicbus
KMOD=	icee
SRCS=	icee.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	ofw_bus_if.h \
	opt_platform.h \

.include <bsd.kmod.mk>
