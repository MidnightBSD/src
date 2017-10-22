# $FreeBSD: stable/9/sys/modules/ata/atacard/Makefile 144330 2005-03-30 12:03:40Z sos $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atacard
SRCS=	ata-card.c
SRCS+=  opt_ata.h pccarddevs.h ata_if.h device_if.h bus_if.h isa_if.h card_if.h

.include <bsd.kmod.mk>
