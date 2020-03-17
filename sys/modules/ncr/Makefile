# $FreeBSD: stable/11/sys/modules/ncr/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/ncr

KMOD=	ncr
SRCS=	ncr.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_ncr.h opt_cam.h

.include <bsd.kmod.mk>
