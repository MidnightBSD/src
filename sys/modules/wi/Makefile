# $FreeBSD: stable/9/sys/modules/wi/Makefile 182251 2008-08-27 05:43:46Z imp $

.PATH: ${.CURDIR}/../../dev/wi

KMOD=	if_wi
SRCS=	if_wi.c if_wi_pccard.c if_wi_pci.c \
	card_if.h device_if.h bus_if.h pci_if.h pccarddevs.h

.include <bsd.kmod.mk>
