# $FreeBSD: release/10.0.0/sys/boot/fdt/Makefile 209920 2010-07-12 00:49:22Z nwhitehorn $

.PATH:		${.CURDIR}/../../contrib/libfdt/

LIB=		fdt
INTERNALLIB=

# Vendor sources of libfdt.
SRCS+=		fdt.c fdt_ro.c fdt_wip.c fdt_sw.c fdt_rw.c fdt_strerror.c

# Loader's fdt commands extension sources.
SRCS+=		fdt_loader_cmd.c

CFLAGS+=	-I${.CURDIR}/../../contrib/libfdt/ -I${.CURDIR}/../common/ \
		-I${.CURDIR}/../uboot/lib

CFLAGS+=	-ffreestanding

.if ${MACHINE_CPUARCH} == "powerpc" || ${MACHINE_CPUARCH} == "arm"
CFLAGS+=	-msoft-float
.endif

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32
.endif

CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
