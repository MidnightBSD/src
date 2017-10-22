# $FreeBSD: release/10.0.0/sys/modules/dtrace/dtrace_test/Makefile 253996 2013-08-06 15:51:56Z avg $

.PATH: ${.CURDIR}/../../../cddl/dev/dtrace

KMOD=		dtrace_test
SRCS=		dtrace_test.c

SRCS+=		opt_kdtrace.h
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../..

CFLAGS+=	-D_KERNEL

.include <bsd.kmod.mk>

CFLAGS+=	-include ${.CURDIR}/../../../cddl/compat/opensolaris/sys/debug_compat.h
