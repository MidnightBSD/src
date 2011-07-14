# $MidnightBSD: src/sys/modules/drm/radeon/Makefile,v 1.3 2011/03/13 21:58:41 laffer1 Exp $
# $FreeBSD: src/sys/modules/drm/radeon/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
SRCS	= r300_cmdbuf.c r600_blit.c r600_cp.c radeon_cp.c \
	radeon_cs.c radeon_drv.c radeon_irq.c \
	radeon_mem.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
