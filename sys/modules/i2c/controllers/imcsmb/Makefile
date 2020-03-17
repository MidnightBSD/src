#$FreeBSD: stable/11/sys/modules/i2c/controllers/imcsmb/Makefile 330304 2018-03-03 01:53:51Z rpokala $

.PATH:		${SRCTOP}/sys/dev/imcsmb
KMOD		= imcsmb
SRCS		= device_if.h bus_if.h pci_if.h smbus_if.h \
		  imcsmb.c imcsmb_pci.c imcsmb_reg.h imcsmb_var.h

.include <bsd.kmod.mk>
