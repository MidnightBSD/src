# $FreeBSD: stable/11/sys/modules/drm/sis/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD= 	sis
SRCS=	sis_drv.c sis_ds.c sis_mm.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
