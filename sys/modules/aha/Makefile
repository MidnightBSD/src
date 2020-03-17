# $FreeBSD: stable/11/sys/modules/aha/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/aha

KMOD=	aha
SRCS=	aha.c aha_isa.c ahareg.h opt_cam.h device_if.h bus_if.h \
	isa_if.h

.include <bsd.kmod.mk>
