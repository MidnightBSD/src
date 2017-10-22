# $FreeBSD: release/7.0.0/sys/modules/ral/Makefile 156322 2006-03-05 20:38:55Z damien $

.PATH: ${.CURDIR}/../../dev/ral

KMOD    = if_ral
SRCS    = rt2560.c rt2661.c if_ralrate.c if_ral_pci.c \
          device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
