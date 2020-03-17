# $FreeBSD: stable/11/sys/modules/sym/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sym

KMOD=	sym
SRCS=	sym_hipd.c opt_sym.h opt_cam.h opt_scsi.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
