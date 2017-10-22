# $FreeBSD: stable/9/sys/modules/dtrace/dtrace_test/Makefile 179065 2008-05-17 02:31:19Z jb $

.PATH: ${.CURDIR}/../../../cddl/dev/dtrace

KMOD=		dtrace_test
SRCS=		dtrace_test.c

SRCS+=		vnode_if.h
		
CFLAGS+=	-I${.CURDIR}/../../..

CFLAGS+=	-D_KERNEL

.include <bsd.kmod.mk>
