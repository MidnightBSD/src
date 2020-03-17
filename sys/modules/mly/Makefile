# $FreeBSD: stable/11/sys/modules/mly/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mly

KMOD=	mly
SRCS=	mly.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS+=	-DMLY_DEBUG=1
CFLAGS+=	-DMLY_MODULE

.include <bsd.kmod.mk>
