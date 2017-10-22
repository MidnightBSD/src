# $FreeBSD: release/10.0.0/sys/modules/i2c/smbus/Makefile 93023 2002-03-23 15:49:15Z nsouch $

.PATH:		${.CURDIR}/../../../dev/smbus
KMOD		= smbus
SRCS		= device_if.h bus_if.h smbus_if.h smbus_if.c \
		  smbconf.h smbconf.c smbus.h smbus.c

.include <bsd.kmod.mk>
