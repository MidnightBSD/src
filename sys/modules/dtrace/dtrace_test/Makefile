# $FreeBSD: stable/11/sys/modules/dtrace/dtrace_test/Makefile 326531 2017-12-04 15:19:50Z markj $

SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/dev/dtrace

KMOD=		dtrace_test
SRCS=		dtrace_test.c

SRCS+=		vnode_if.h

CFLAGS+=	-I${SYSDIR}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
