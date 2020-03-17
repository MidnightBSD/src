# $FreeBSD: stable/11/sys/modules/drm/radeon/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD	= radeon
SRCS	= r300_cmdbuf.c r600_blit.c r600_cp.c radeon_cp.c radeon_cs.c \
	radeon_drv.c radeon_irq.c radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

CWARNFLAGS.r600_cp.c=	-Wno-unused ${NO_WCONSTANT_CONVERSION}
CWARNFLAGS.radeon_cp.c=	-Wno-unused ${NO_WCONSTANT_CONVERSION}
