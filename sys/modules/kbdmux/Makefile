# $Id: Makefile,v 1.1 2005/06/23 00:30:50 max Exp $
# $FreeBSD: stable/11/sys/modules/kbdmux/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/kbdmux

KMOD=	kbdmux
SRCS=	kbdmux.c opt_compat.h opt_evdev.h opt_kbd.h opt_kbdmux.h bus_if.h \
	device_if.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
