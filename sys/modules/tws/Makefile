# Makefile for tws (LSI 3ware 9750 SAS2/SATA-II RAID PCIe) driver
# $FreeBSD: release/10.0.0/sys/modules/tws/Makefile 226026 2011-10-04 21:40:25Z delphij $

KMOD=	tws
.PATH:	${.CURDIR}/../../dev/${KMOD}
 
SRCS=	tws.c tws_services.c tws_cam.c tws_hdm.c tws_user.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_cam.h opt_scsi.h

.include <bsd.kmod.mk>
