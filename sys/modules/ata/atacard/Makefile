# $FreeBSD: stable/11/sys/modules/ata/atacard/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/ata

KMOD=	atacard
SRCS=	ata-card.c
SRCS+=	ata_if.h bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
