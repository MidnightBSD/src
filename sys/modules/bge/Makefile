# $FreeBSD: stable/11/sys/modules/bge/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/bge

KMOD=	if_bge
SRCS=	if_bge.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

.if ${MACHINE_CPUARCH} == sparc64
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
