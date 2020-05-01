# $FreeBSD: stable/11/gnu/usr.bin/gdb/Makefile 276551 2015-01-02 18:45:03Z bapt $

SUBDIR=	libgdb gdb gdbtui kgdb

.if exists(${.CURDIR}/gdbserver/reg-${MACHINE_CPUARCH}.c)
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
