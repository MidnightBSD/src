# $FreeBSD: stable/11/sys/modules/aac/Makefile 331636 2018-03-27 17:51:45Z brooks $

.PATH: ${SRCTOP}/sys/dev/aac

.if ${MACHINE_CPUARCH} == "i386"
SUBDIR= aac_linux
.endif

KMOD=	aac
SRCS=	aac.c aac_pci.c aac_disk.c aac_cam.c
SRCS+=	opt_scsi.h opt_cam.h opt_compat.h opt_aac.h
SRCS+=	device_if.h bus_if.h pci_if.h

# To enable debug output from the driver, uncomment these two lines.
#CFLAGS+= -DAAC_DEBUG=2
#SRCS+=	aac_debug.c

.include <bsd.kmod.mk>
