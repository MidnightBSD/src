# $FreeBSD: stable/11/sys/modules/drm/r128/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD	= r128
SRCS	= r128_cce.c r128_drv.c r128_irq.c r128_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

CWARNFLAGS.r128_cce.c=		-Wno-unused ${NO_WCONSTANT_CONVERSION}
CWARNFLAGS.r128_state.c=	-Wno-unused
