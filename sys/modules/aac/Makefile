# $FreeBSD: release/10.0.0/sys/modules/aac/Makefile 211690 2010-08-23 06:13:29Z imp $

.PATH: ${.CURDIR}/../../dev/aac

.if ${MACHINE_CPUARCH} == "i386"
SUBDIR= aac_linux
.endif

KMOD=	aac
SRCS=	aac.c aac_pci.c aac_disk.c aac_cam.c
SRCS+=	opt_scsi.h opt_cam.h opt_aac.h
SRCS+=	device_if.h bus_if.h pci_if.h

# To enable debug output from the driver, uncomment these two lines.
#CFLAGS+= -DAAC_DEBUG=2
#SRCS+=	aac_debug.c

.include <bsd.kmod.mk>
