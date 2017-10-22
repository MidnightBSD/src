# $FreeBSD: release/7.0.0/sys/modules/drm/r128/Makefile 124472 2004-01-13 11:28:51Z ru $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= r128
SRCS	= r128_cce.c r128_drv.c r128_irq.c r128_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
