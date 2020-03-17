# $FreeBSD: stable/11/sys/modules/bwi/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/bwi

KMOD	= if_bwi

SRCS	= if_bwi.c if_bwi_pci.c bwimac.c bwiphy.c bwirf.c
SRCS	+= device_if.h bus_if.h pci_if.h opt_inet.h opt_bwi.h opt_wlan.h

.include <bsd.kmod.mk>
