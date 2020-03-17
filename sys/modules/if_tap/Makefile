# Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
#
# $FreeBSD: stable/11/sys/modules/if_tap/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/net

KMOD=	if_tap
SRCS=	if_tap.c opt_compat.h opt_inet.h vnode_if.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
