# $FreeBSD: stable/11/sys/modules/sbni/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sbni

KMOD=	if_sbni
SRCS=	if_sbni.c if_sbni_isa.c if_sbni_pci.c

SRCS+=	bus_if.h device_if.h isa_if.h pci_if.h

.include <bsd.kmod.mk>
