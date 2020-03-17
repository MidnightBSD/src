# $FreeBSD: stable/11/sys/modules/hptiop/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/hptiop

KMOD=	hptiop
SRCS=	hptiop.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
