# $FreeBSD: stable/9/sys/modules/bwi/Makefile 191762 2009-05-03 04:01:43Z imp $

.PATH: ${.CURDIR}/../../dev/bwi

KMOD	= if_bwi

SRCS	= if_bwi.c if_bwi_pci.c bwimac.c bwiphy.c bwirf.c
SRCS	+= device_if.h bus_if.h pci_if.h opt_inet.h opt_bwi.h

opt_bwi.h:
	echo '#define BWI_DEBUG 1' > opt_bwi.h
#	echo '#define BWI_DEBUG_VERBOSE 1' >> opt_bwi.h

.include <bsd.kmod.mk>
