# $FreeBSD: stable/9/sys/modules/dtrace/fasttrap/Makefile 211746 2010-08-24 12:18:39Z rpaulo $

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
