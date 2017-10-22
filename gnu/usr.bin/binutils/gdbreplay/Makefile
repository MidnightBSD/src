# $FreeBSD: release/7.0.0/gnu/usr.bin/binutils/gdbreplay/Makefile 139103 2004-12-21 08:47:35Z ru $

.include "../Makefile.inc0"

# Not elf specific so don't install in /usr/libexec/elf
BINDIR=/usr/bin

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb

PROG=	gdbreplay
NO_MAN=

SRCS=	gdbreplay.c

CFLAGS+=	-I${.CURDIR}/../gdb
CFLAGS+=	-I${GDBDIR}/gdb
CFLAGS+=	-I${GDBDIR}/gdb/config
CFLAGS+=	-I${GDBDIR}/gdb/gdbserver
CFLAGS+=	-DNO_MMALLOC

.include <bsd.prog.mk>
