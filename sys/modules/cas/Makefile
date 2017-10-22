# $FreeBSD: release/10.0.0/sys/modules/cas/Makefile 209991 2010-07-13 12:47:31Z nwhitehorn $

.PATH: ${.CURDIR}/../../dev/cas

KMOD=	if_cas
SRCS=	bus_if.h device_if.h if_cas.c miibus_if.h pci_if.h ${ofw_bus_if}

.if ${MACHINE_CPUARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
ofw_bus_if=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
