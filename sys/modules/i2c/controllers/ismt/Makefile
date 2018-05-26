# $MidnightBSD$
#$FreeBSD: stable/10/sys/modules/i2c/controllers/ismt/Makefile 293675 2016-01-11 17:53:24Z jimharris $

.PATH:		${.CURDIR}/../../../../dev/ismt
KMOD		= ismt
SRCS		= device_if.h bus_if.h iicbb_if.h pci_if.h smbus_if.h \
		  ismt.c

.include <bsd.kmod.mk>
