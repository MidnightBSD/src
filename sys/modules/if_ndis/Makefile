# $FreeBSD: release/7.0.0/sys/modules/if_ndis/Makefile 150636 2005-09-27 18:10:43Z mlaier $

.PATH: ${.CURDIR}/../../dev/if_ndis

KMOD=	if_ndis
SRCS=	if_ndis.c if_ndis_pci.c if_ndis_pccard.c if_ndis_usb.c
SRCS+=	device_if.h bus_if.h pci_if.h card_if.h pccarddevs.h
SRCS+=	opt_usb.h usbdevs.h

.include <bsd.kmod.mk>
