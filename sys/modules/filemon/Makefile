# $FreeBSD: stable/11/sys/modules/filemon/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/filemon

KMOD=	filemon
SRCS=	${KMOD}.c
SRCS+=	opt_compat.h vnode_if.h

.include <bsd.kmod.mk>
