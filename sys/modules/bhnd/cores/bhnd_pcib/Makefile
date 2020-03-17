# $FreeBSD: stable/11/sys/modules/bhnd/cores/bhnd_pcib/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/bhnd/cores/pci
.PATH: ${SRCTOP}/sys/dev/bhnd/cores/pcie2

KMOD=	bhnd_pcib
SRCS=	bhnd_pcib.c bhnd_pcie2b.c
SRCS+=	device_if.h bus_if.h pci_if.h \
	bhnd_bus_if.h

.include <bsd.kmod.mk>
