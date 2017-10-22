# $FreeBSD: stable/9/sys/modules/mqueue/Makefile 209442 2010-06-22 20:52:35Z thompsa $

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h opt_compat.h

.include <bsd.kmod.mk>
