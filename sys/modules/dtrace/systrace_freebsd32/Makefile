# $FreeBSD: stable/9/sys/modules/dtrace/systrace_freebsd32/Makefile 219561 2011-03-12 09:09:25Z avg $

.PATH: ${.CURDIR}/../../../cddl/dev/systrace

KMOD=		systrace_freebsd32
SRCS=		systrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../.. -DFREEBSD32_SYSTRACE

.include <bsd.kmod.mk>
