# $FreeBSD: stable/11/sys/modules/cs/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/cs

KMOD=	if_cs
SRCS=	if_cs.c if_cs_isa.c if_cs_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h isa_if.h pccarddevs.h

.include <bsd.kmod.mk>
