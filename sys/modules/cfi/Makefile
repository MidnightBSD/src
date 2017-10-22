# $FreeBSD: release/10.0.0/sys/modules/cfi/Makefile 229032 2011-12-30 21:22:10Z marius $

.PATH: ${.CURDIR}/../../dev/cfi

KMOD=   cfi
SRCS=   ${_cfi_bus} cfi_core.c cfi_dev.c
SRCS+=  bus_if.h device_if.h opt_cfi.h

.if ${MACHINE} == "arm"
_cfi_bus=	cfi_bus_fdt.c cfi_bus_ixp4xx.c ofw_bus_if.h
.endif
.if ${MACHINE} == "powerpc"
_cfi_bus=	cfi_bus_fdt.c ofw_bus_if.h
.endif

opt_cfi.h:
	echo "#define CFI_SUPPORT_STRATAFLASH 1" > ${.TARGET}

.include <bsd.kmod.mk>
