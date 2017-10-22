# $FreeBSD: release/10.0.0/sys/modules/i2c/controllers/viapm/Makefile 171020 2007-06-24 20:35:59Z njl $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= viapm
SRCS		= device_if.h bus_if.h iicbb_if.h isa_if.h pci_if.h smbus_if.h \
		  opt_isa.h viapm.c

.include <bsd.kmod.mk>
