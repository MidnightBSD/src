# $FreeBSD: release/10.0.0/sys/modules/asr/Makefile 228865 2011-12-24 17:54:58Z dim $

.PATH: ${.CURDIR}/../../dev/asr

KMOD=	asr
SRCS=	asr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	opt_asr.h
.endif

.include <bsd.kmod.mk>

CWARNFLAGS.asr.c=	${NO_WARRAY_BOUNDS}
CWARNFLAGS+=		${CWARNFLAGS.${.IMPSRC:T}}
