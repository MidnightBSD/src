# $MidnightBSD: src/sys/modules/cyclic/Makefile,v 1.1 2012/03/31 16:59:45 laffer1 Exp $
# $FreeBSD: src/sys/modules/cyclic/Makefile,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../cddl/dev/cyclic

KMOD=		cyclic
SRCS=		cyclic.c

SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..					\
		-I${.CURDIR}/../../cddl/dev/cyclic/${MACHINE_CPUARCH:S/amd64/i386/}

CFLAGS+=	-DDEBUG=1

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
