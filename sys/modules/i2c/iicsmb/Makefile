# $FreeBSD: stable/11/sys/modules/i2c/iicsmb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:		${SRCTOP}/sys/dev/iicbus
KMOD		= iicsmb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  smbus_if.h iicsmb.c

.include <bsd.kmod.mk>
