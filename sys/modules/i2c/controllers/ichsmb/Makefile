#$FreeBSD: stable/11/sys/modules/i2c/controllers/ichsmb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:		${SRCTOP}/sys/dev/ichsmb
KMOD		= ichsmb
SRCS		= device_if.h bus_if.h iicbb_if.h pci_if.h smbus_if.h \
		  ichsmb.c ichsmb_pci.c ichsmb_reg.h ichsmb_var.h

.include <bsd.kmod.mk>
