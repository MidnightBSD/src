# $FreeBSD: stable/9/sys/modules/filemon/Makefile 242112 2012-10-26 00:44:32Z eadler $

.PATH: ${.CURDIR}/../../dev/filemon

KMOD=	filemon
SRCS=	${KMOD}.c
SRCS+=	vnode_if.h opt_compat.h opt_capsicum.h

.include <bsd.kmod.mk>
