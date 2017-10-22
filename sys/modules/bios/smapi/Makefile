# $FreeBSD: release/7.0.0/sys/modules/bios/smapi/Makefile 140867 2005-01-26 16:29:07Z imp $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
