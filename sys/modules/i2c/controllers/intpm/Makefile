# $FreeBSD: stable/11/sys/modules/i2c/controllers/intpm/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:		${SRCTOP}/sys/dev/intpm
KMOD		= intpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  opt_intpm.h intpmreg.h intpm.c

.include <bsd.kmod.mk>
