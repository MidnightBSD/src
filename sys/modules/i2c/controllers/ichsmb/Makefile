#$FreeBSD: release/10.0.0/sys/modules/i2c/controllers/ichsmb/Makefile 146996 2005-06-05 11:55:29Z takawata $

.PATH:		${.CURDIR}/../../../../dev/ichsmb
KMOD		= ichsmb
SRCS		= device_if.h bus_if.h iicbb_if.h pci_if.h smbus_if.h \
		  ichsmb.c ichsmb_pci.c ichsmb_reg.h ichsmb_var.h

.include <bsd.kmod.mk>
