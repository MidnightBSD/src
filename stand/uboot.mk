# $FreeBSD: stable/11/stand/uboot.mk 332150 2018-04-06 20:24:50Z kevans $

SRCS+=	main.c

.PATH:		${UBOOTSRC}/common

CFLAGS+=	-I${UBOOTSRC}/common

# U-Boot standalone support library
LIBUBOOT=	${BOOTOBJ}/uboot/lib/libuboot.a
CFLAGS+=	-I${UBOOTSRC}/lib
CFLAGS+=	-I${BOOTOBJ}/uboot/lib
.if ${MACHINE_CPUARCH} == "arm"
SRCS+=	metadata.c
.endif

.include "${BOOTSRC}/fdt.mk"

.if ${MK_FDT} == "yes"
LIBUBOOT_FDT=	${BOOTOBJ}/uboot/fdt/libuboot_fdt.a
.endif
