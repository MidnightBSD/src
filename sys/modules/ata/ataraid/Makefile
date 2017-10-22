# $FreeBSD: stable/9/sys/modules/ata/ataraid/Makefile 144330 2005-03-30 12:03:40Z sos $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	ataraid
SRCS=	ata-raid.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
