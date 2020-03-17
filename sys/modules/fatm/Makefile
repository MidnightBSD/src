# $FreeBSD: stable/11/sys/modules/fatm/Makefile 319182 2017-05-30 04:11:12Z ngie $
#
# Author: Harti Brandt <harti@freebsd.org>
#
.PATH: ${SRCTOP}/sys/dev/fatm

KMOD=	if_fatm
SRCS=	if_fatm.c device_if.h bus_if.h pci_if.h opt_inet.h opt_natm.h 

# CFLAGS+= -DFATM_DEBUG=0 -DINVARIANT_SUPPORT -DINVARIANTS

.include <bsd.kmod.mk>
