# $FreeBSD: stable/9/sys/modules/amd/Makefile 107840 2002-12-13 22:59:18Z scottl $

.PATH: ${.CURDIR}/../../dev/amd

KMOD=	amd
SRCS=	amd.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
