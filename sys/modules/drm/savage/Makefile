# $FreeBSD: stable/11/sys/modules/drm/savage/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/drm
KMOD= 	savage
CFLAGS += -I${SRCTOP}/sys/dev/drm
SRCS=	savage_bci.c savage_drv.c savage_state.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
