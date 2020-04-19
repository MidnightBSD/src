# $FreeBSD: stable/11/stand/libsa/zfs/Makefile.inc 336084 2018-07-08 07:42:49Z imp $

.PATH:		${ZFSSRC}
SRCS+=		zfs.c skein.c skein_block.c
# Do not unroll skein loops, reduce code size
CFLAGS+=	-DSKEIN_LOOP=111
.PATH:		${SYSDIR}/crypto/skein

CFLAGS+=	-I${LDRSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/crypto/skein

CFLAGS+=	-Wformat -Wall
