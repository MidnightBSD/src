# $FreeBSD: stable/11/sys/modules/drm/i915/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD	= i915
SRCS	= i915_dma.c i915_drv.c i915_irq.c i915_mem.c i915_suspend.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
