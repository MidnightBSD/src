# $FreeBSD: stable/11/sys/modules/ata/atapci/Makefile 319182 2017-05-30 04:11:12Z ngie $

SUBDIR += chipsets

.PATH: ${SRCTOP}/sys/dev/ata

KMOD=	atapci
SRCS=	ata-pci.c ata-dma.c ata-sata.c
SRCS+=	ata_if.h bus_if.h device_if.h pci_if.h

.include <bsd.kmod.mk>
