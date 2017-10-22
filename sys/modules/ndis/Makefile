# $FreeBSD: release/10.0.0/sys/modules/ndis/Makefile 211690 2010-08-23 06:13:29Z imp $

.PATH: ${.CURDIR}/../../compat/ndis

KMOD=	ndis
SRCS=	subr_pe.c subr_ndis.c subr_hal.c subr_ntoskrnl.c kern_ndis.c
SRCS+=	kern_windrv.c subr_usbd.c
SRCS+=	device_if.h bus_if.h pci_if.h vnode_if.h
SRCS+=	opt_bus.h opt_usb.h usb_if.h usbdevs.h

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	winx64_wrap.S
.endif

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	winx32_wrap.S
.endif

.include <bsd.kmod.mk>
