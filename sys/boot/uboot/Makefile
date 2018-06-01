# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/uboot/Makefile 275763 2014-12-14 15:33:45Z andrew $

.include <bsd.own.mk>

SUBDIR=	lib

.if ${MK_FDT} != "no"
SUBDIR+=fdt
.endif

.include <bsd.subdir.mk>
