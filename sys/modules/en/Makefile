# $FreeBSD: stable/11/sys/modules/en/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/en

KMOD=	if_en
SRCS=	if_en_pci.c midway.c device_if.h bus_if.h pci_if.h
SRCS+=	opt_inet.h opt_natm.h opt_ddb.h
CFLAGS+= -DENABLE_BPF
# CFLAGS+= -DINVARIANT_SUPPORT -DINVARIANTS -DEN_DEBUG=0

.include <bsd.kmod.mk>
