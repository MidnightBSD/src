# $FreeBSD: stable/11/sys/modules/i2c/controllers/viapm/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:		${SRCTOP}/sys/dev/viapm
KMOD		= viapm
SRCS		= device_if.h bus_if.h iicbb_if.h isa_if.h pci_if.h smbus_if.h \
		  opt_isa.h viapm.c

.include <bsd.kmod.mk>
