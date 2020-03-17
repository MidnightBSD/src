# $FreeBSD: stable/11/sys/modules/dtrace/systrace/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/dev/systrace

KMOD=		systrace
SRCS=		systrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${SYSDIR}/cddl/compat/opensolaris \
		-I${SYSDIR}/cddl/contrib/opensolaris/uts/common \
		-I${SYSDIR}/cddl/contrib/opensolaris/uts/common/dtrace \
		-I${SYSDIR}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
