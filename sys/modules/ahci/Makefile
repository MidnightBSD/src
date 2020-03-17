# $FreeBSD: stable/11/sys/modules/ahci/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/ahci

KMOD=	ahci
SRCS=	ahci.c ahci_pci.c ahciem.c ahci.h device_if.h bus_if.h pci_if.h opt_cam.h

.if ${MACHINE_CPUARCH} == "aarch64"
SRCS+=	ahci_generic.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
