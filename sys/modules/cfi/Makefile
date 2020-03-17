# $FreeBSD: stable/11/sys/modules/cfi/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/cfi

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
