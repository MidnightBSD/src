# $FreeBSD: release/10.0.0/sys/modules/ata/atacore/Makefile 249213 2013-04-06 19:12:49Z marius $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	ata
SRCS=	ata-all.c ata_if.c ata-lowlevel.c
SRCS+=	ata_if.h bus_if.h device_if.h opt_cam.h pci_if.h

.include <bsd.kmod.mk>
