# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/userboot/zfs/Makefile 262331 2014-02-22 07:18:06Z grehan $

S=		${.CURDIR}/../../zfs

.PATH:		${S}
LIB=		zfsboot
INTERNALLIB=

SRCS+=		zfs.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs

CFLAGS+=	-ffreestanding -fPIC
CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
