# $MidnightBSD: src/sys/modules/dtrace/fasttrap/Makefile,v 1.1 2012/03/28 21:18:51 laffer1 Exp $
# $FreeBSD: src/sys/modules/dtrace/fasttrap/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common/dtrace

KMOD=		fasttrap
SRCS=		fasttrap.c fasttrap_isa.c opt_compat.h
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.if ${MACHINE_CPUARCH} == "amd64" ||  ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/intel
.PATH:		${.CURDIR}/../../../cddl/contrib/opensolaris/uts/intel/dtrace
.endif

CFLAGS+=	-DSMP -DDEBUG

.include <bsd.kmod.mk>
