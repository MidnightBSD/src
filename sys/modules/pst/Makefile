# $FreeBSD: release/10.0.0/sys/modules/pst/Makefile 119571 2003-08-30 08:01:05Z sos $

.PATH: ${.CURDIR}/../../dev/pst

KMOD=	pst
SRCS=	pst-pci.c pst-iop.c pst-raid.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
