# $FreeBSD: release/10.0.0/sys/modules/mqueue/Makefile 247719 2013-03-03 16:17:09Z uqs $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h opt_compat.h opt_capsicum.h

.include <bsd.kmod.mk>
