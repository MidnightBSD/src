# $FreeBSD: stable/11/sys/modules/an/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/an

KMOD=	if_an
SRCS=	if_an.c if_an_pccard.c if_an_pci.c if_an_isa.c
SRCS+=	opt_inet.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h
SRCS+=	pccarddevs.h

.include <bsd.kmod.mk>
