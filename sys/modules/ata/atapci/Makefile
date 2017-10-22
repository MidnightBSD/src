# $FreeBSD: release/10.0.0/sys/modules/ata/atapci/Makefile 249213 2013-04-06 19:12:49Z marius $

SUBDIR += chipsets

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapci
SRCS=	ata-pci.c ata-dma.c ata-sata.c
SRCS+=	ata_if.h bus_if.h device_if.h pci_if.h

.include <bsd.kmod.mk>
