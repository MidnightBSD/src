# $FreeBSD: release/10.0.0/sys/modules/sbni/Makefile 86752 2001-11-21 22:29:35Z fjoe $

.PATH: ${.CURDIR}/../../dev/sbni

KMOD=	if_sbni
SRCS=	if_sbni.c if_sbni_isa.c if_sbni_pci.c

SRCS+=	bus_if.h device_if.h isa_if.h pci_if.h

.include <bsd.kmod.mk>
