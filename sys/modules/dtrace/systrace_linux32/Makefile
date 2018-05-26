# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/dtrace/systrace_linux32/Makefile 294368 2016-01-20 01:09:53Z jhb $

.PATH: ${.CURDIR}/../../../cddl/dev/systrace

KMOD=		systrace_linux32

SRCS=		systrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../.. -DLINUX32_SYSTRACE

.include <bsd.kmod.mk>

CFLAGS+=	-include ${.CURDIR}/../../../cddl/compat/opensolaris/sys/debug_compat.h
