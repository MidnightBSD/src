# $FreeBSD: stable/11/sys/modules/drm/mga/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD	= mga
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c \
	mga_irq.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
