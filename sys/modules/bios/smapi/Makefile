# $FreeBSD: stable/9/sys/modules/bios/smapi/Makefile 232930 2012-03-13 18:39:57Z dim $
#

.PATH:  ${.CURDIR}/../../../i386/bios

KMOD=	smapi
SRCS=	smapi.c smapi_bios.S \
	bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>

# XXX: clang integrated-as doesn't grok 16-bit assembly yet
CFLAGS.smapi_bios.S=	${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}
