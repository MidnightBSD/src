# $FreeBSD: stable/11/sys/modules/ata/atacore/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/ata

KMOD=	ata
SRCS=	ata-all.c ata_if.c ata-lowlevel.c
SRCS+=	ata_if.h bus_if.h device_if.h opt_cam.h pci_if.h

.include <bsd.kmod.mk>
