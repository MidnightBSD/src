# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/i2c/iicbb/Makefile 266105 2014-05-15 01:27:53Z loos $

.PATH:		${.CURDIR}/../../../dev/iicbus
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c opt_platform.h

.include <bsd.kmod.mk>
