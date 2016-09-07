# $MidnightBSD$
# $FreeBSD: release/9.2.0/sys/modules/dtrace/dtio/Makefile 238366 2012-07-11 16:27:02Z gnn $

.PATH: ${.CURDIR}/../../../kern

KMOD=		dtio
SRCS=		dtio_kdtrace.c \
		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
