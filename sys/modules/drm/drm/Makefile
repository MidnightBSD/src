# $FreeBSD: stable/11/sys/modules/drm/drm/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD	= drm
SRCS	= \
	ati_pcigart.c \
	drm_agpsupport.c \
	drm_auth.c \
	drm_bufs.c \
	drm_context.c \
	drm_dma.c \
	drm_drawable.c \
	drm_drv.c \
	drm_fops.c \
	drm_hashtab.c \
	drm_ioctl.c \
	drm_irq.c \
	drm_lock.c \
	drm_memory.c \
	drm_mm.c \
	drm_pci.c \
	drm_scatter.c \
	drm_sman.c \
	drm_sysctl.c \
	drm_vm.c

SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
