# $FreeBSD: stable/11/sys/modules/iir/Makefile 319182 2017-05-30 04:11:12Z ngie $

KMOD		=  iir
.PATH:		${SRCTOP}/sys/dev/${KMOD}
SRCS		=  iir.c iir_ctrl.c iir_pci.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
