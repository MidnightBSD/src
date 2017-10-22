# $FreeBSD: release/10.0.0/sys/modules/dtrace/dtio/Makefile 253996 2013-08-06 15:51:56Z avg $

.PATH: ${.CURDIR}/../../../kern

KMOD=		dtio
SRCS=		dtio_kdtrace.c \
		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>

CFLAGS+=	-include ${.CURDIR}/../../../cddl/compat/opensolaris/sys/debug_compat.h
