# $FreeBSD: release/10.0.0/sys/modules/arcmsr/Makefile 144411 2005-03-31 18:19:55Z scottl $

.PATH: ${.CURDIR}/../../dev/arcmsr

KMOD=	arcmsr
SRCS=	arcmsr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
