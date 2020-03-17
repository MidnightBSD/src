# $Id: Makefile,v 1.1 2004/08/13 18:30:24 max Exp $
# $FreeBSD: stable/11/sys/modules/vkbd/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/vkbd

KMOD=	vkbd
SRCS=	vkbd.c opt_compat.h opt_kbd.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
