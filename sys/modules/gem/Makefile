# $FreeBSD: stable/11/sys/modules/gem/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/gem

KMOD=	if_gem
SRCS=	bus_if.h device_if.h if_gem.c if_gem_pci.c ${if_gem_sbus} miibus_if.h
SRCS+=	${ofw_bus_if} pci_if.h

.if ${MACHINE} == "sparc64"
if_gem_sbus=	if_gem_sbus.c
.endif

.if ${MACHINE_CPUARCH} == "powerpc" || ${MACHINE_CPUARCH} == "sparc64"
ofw_bus_if=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
