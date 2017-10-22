# $FreeBSD: release/7.0.0/gnu/usr.bin/gdb/Makefile 163582 2006-10-21 17:27:36Z jmg $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
