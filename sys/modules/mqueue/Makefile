# $FreeBSD: stable/11/sys/modules/mqueue/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h opt_compat.h opt_capsicum.h

.include <bsd.kmod.mk>
