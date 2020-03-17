# $FreeBSD: stable/11/sys/modules/xe/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/xe

KMOD=	if_xe
SRCS=	if_xe.c if_xe_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
