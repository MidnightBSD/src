# $FreeBSD: stable/11/stand/fdt.mk 329145 2018-02-12 01:08:44Z kevans $

.if ${MK_FDT} == "yes"
CFLAGS+=	-I${FDTSRC}
CFLAGS+=	-I${BOOTOBJ}/fdt
CFLAGS+=	-I${SYSDIR}/contrib/libfdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBFDT=		${BOOTOBJ}/fdt/libfdt.a
.endif
