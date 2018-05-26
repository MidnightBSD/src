# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/filemon/Makefile 296015 2016-02-24 22:30:22Z bdrewery $

.PATH: ${.CURDIR}/../../dev/filemon

KMOD=	filemon
SRCS=	${KMOD}.c
SRCS+=	opt_compat.h vnode_if.h

.include <bsd.kmod.mk>
