# $FreeBSD: stable/9/sys/modules/cyclic/Makefile 211690 2010-08-23 06:13:29Z imp $

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
