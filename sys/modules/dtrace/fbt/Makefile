# $FreeBSD: release/10.0.0/sys/modules/dtrace/fbt/Makefile 253996 2013-08-06 15:51:56Z avg $

.PATH: ${.CURDIR}/../../../cddl/dev/fbt

KMOD=		fbt
.if ${MACHINE_CPUARCH} == "powerpc"
SRCS=		fbt_powerpc.c
.else
SRCS=		fbt.c
.endif
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>

CFLAGS+=	-include ${.CURDIR}/../../../cddl/compat/opensolaris/sys/debug_compat.h
