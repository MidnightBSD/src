# $FreeBSD: stable/9/sys/modules/ata/atapicam/Makefile 144645 2005-04-05 02:04:30Z scottl $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapicam
SRCS=	atapi-cam.c
SRCS+=  opt_ata.h opt_cam.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
