# $FreeBSD: stable/11/sys/modules/bios/smapi/Makefile 319182 2017-05-30 04:11:12Z ngie $
#

.PATH:  ${SRCTOP}/sys/i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h

.include <bsd.kmod.mk>

CFLAGS.smapi_bios.S=	${CLANG_NO_IAS34}
