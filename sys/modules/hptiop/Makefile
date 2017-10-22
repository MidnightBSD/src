# $FreeBSD: release/10.0.0/sys/modules/hptiop/Makefile 169412 2007-05-09 07:07:26Z scottl $

.PATH: ${.CURDIR}/../../dev/hptiop

KMOD=	hptiop
SRCS=	hptiop.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
