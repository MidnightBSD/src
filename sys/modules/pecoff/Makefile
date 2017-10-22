# $FreeBSD: release/7.0.0/sys/modules/pecoff/Makefile 146935 2005-06-03 15:35:36Z imp $

.PATH: ${.CURDIR}/../../compat/pecoff

KMOD=	pecoff
SRCS=	imgact_pecoff.c opt_pecoff.h vnode_if.h

CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
