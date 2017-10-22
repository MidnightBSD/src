# $FreeBSD: release/10.0.0/sys/modules/joy/Makefile 129740 2004-05-26 00:53:10Z imp $

.PATH: ${.CURDIR}/../../dev/joy

KMOD=	joy
SRCS=	bus_if.h device_if.h isa_if.h card_if.h joy.c joy_isa.c joy_pccard.c \
	pccarddevs.h

.include <bsd.kmod.mk>
