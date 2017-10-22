# $FreeBSD: release/10.0.0/gnu/usr.bin/gdb/Makefile 211725 2010-08-23 22:24:11Z imp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if exists(${.CURDIR}/gdbserver/reg-${MACHINE_CPUARCH}.c)
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
