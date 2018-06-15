# $MidnightBSD$
# $FreeBSD: src/gnu/usr.bin/gdb/Makefile,v 1.12 2006/10/21 17:27:36 jmg Exp $

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if exists(${.CURDIR}/gdbserver/reg-${MACHINE_CPUARCH}.c)
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
