# $FreeBSD: stable/11/sys/modules/pccard/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/pccard

KMOD=	pccard
SRCS=	pccard.c pccard_cis.c pccard_cis_quirks.c pccard_device.c \
	device_if.h bus_if.h card_if.h power_if.h pccarddevs.h

.include <bsd.kmod.mk>
