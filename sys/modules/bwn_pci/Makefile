# $FreeBSD: stable/11/sys/modules/bwn_pci/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/bwn

KMOD=	if_bwn_pci
SRCS=	if_bwn_pci.c bwn_mac.c
SRCS+=	bhnd_bus_if.h bhndb_bus_if.h \
	bhndb_if.h bhnd_nvram_map.h
SRCS+=	device_if.h bus_if.h pci_if.h \
	opt_bwn.h opt_wlan.h opt_global.h

.include <bsd.kmod.mk>
