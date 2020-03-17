# $FreeBSD: stable/11/sys/modules/drm/mach64/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD	= mach64
SRCS	= mach64_dma.c mach64_drv.c mach64_irq.c mach64_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
