# $FreeBSD: release/10.0.0/sys/modules/ral/Makefile 235233 2012-05-10 17:41:16Z bschmidt $

.PATH: ${.CURDIR}/../../dev/ral

KMOD=	if_ral
SRCS=	rt2560.c rt2661.c rt2860.c if_ral_pci.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
