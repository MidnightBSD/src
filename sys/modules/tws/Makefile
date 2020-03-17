# Makefile for tws (LSI 3ware 9750 SAS2/SATA-II RAID PCIe) driver
# $FreeBSD: stable/11/sys/modules/tws/Makefile 319182 2017-05-30 04:11:12Z ngie $

KMOD=	tws
.PATH:	${SRCTOP}/sys/dev/${KMOD}
 
SRCS=	tws.c tws_services.c tws_cam.c tws_hdm.c tws_user.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_cam.h opt_scsi.h

.include <bsd.kmod.mk>
