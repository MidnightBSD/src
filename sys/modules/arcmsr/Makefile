# $FreeBSD: stable/11/sys/modules/arcmsr/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/arcmsr

KMOD=	arcmsr
SRCS=	arcmsr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
