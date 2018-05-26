# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/ahci/Makefile 279918 2015-03-12 13:40:02Z mav $

.PATH: ${.CURDIR}/../../dev/ahci

KMOD=	ahci
SRCS=	ahci.c ahci_pci.c ahciem.c ahci.h device_if.h bus_if.h pci_if.h opt_cam.h

.include <bsd.kmod.mk>
