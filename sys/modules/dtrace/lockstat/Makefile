# $FreeBSD: stable/9/sys/modules/dtrace/lockstat/Makefile 197075 2009-09-10 19:12:08Z n_hibma $

.PATH: ${.CURDIR}/../../../cddl/dev/lockstat

KMOD=		lockstat
SRCS=		lockstat.c 

SRCS+=		vnode_if.h opt_kdtrace.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
