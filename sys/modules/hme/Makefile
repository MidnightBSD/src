# $FreeBSD: stable/11/sys/modules/hme/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/hme

KMOD=	if_hme
SRCS=	bus_if.h device_if.h if_hme.c if_hme_pci.c ${if_hme_sbus} miibus_if.h
SRCS+=	${ofw_bus_if} pci_if.h

.if ${MACHINE} == "sparc64"
if_hme_sbus=	if_hme_sbus.c
ofw_bus_if=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
