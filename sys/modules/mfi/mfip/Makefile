# $FreeBSD: stable/11/sys/modules/mfi/mfip/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mfi

KMOD=	mfip
SRCS=	mfi_cam.c
SRCS+=	opt_mfi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
