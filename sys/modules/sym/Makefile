# $FreeBSD: release/10.0.0/sys/modules/sym/Makefile 70711 2001-01-06 14:00:42Z obrien $

.PATH: ${.CURDIR}/../../dev/sym

KMOD=	sym
SRCS=	sym_hipd.c opt_sym.h opt_cam.h opt_scsi.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
