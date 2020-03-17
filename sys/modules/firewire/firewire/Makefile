# $FreeBSD: stable/11/sys/modules/firewire/firewire/Makefile 319182 2017-05-30 04:11:12Z ngie $

# Makefile for the IEEE1394 OHCI chipset

.PATH: ${SRCTOP}/sys/dev/firewire

KMOD= firewire
SRCS	= bus_if.h device_if.h pci_if.h \
	  firewire.c firewire.h firewire_phy.h firewirereg.h \
	  fwohci.c fwohci_pci.c fwohcireg.h fwohcivar.h \
	  iec13213.h iec68113.h \
	  fwcrom.c fwdev.c fwmem.c fwmem.h fwdma.c fwdma.h

.include <bsd.kmod.mk>
