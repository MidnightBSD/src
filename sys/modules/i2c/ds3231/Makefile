# $FreeBSD: stable/11/sys/modules/i2c/ds3231/Makefile 323929 2017-09-22 15:40:56Z ian $

.PATH:		${SRCTOP}/sys/dev/iicbus
KMOD		= ds3231
SRCS		= ds3231.c bus_if.h clock_if.h device_if.h iicbus_if.h ofw_bus_if.h opt_platform.h

.include <bsd.kmod.mk>
