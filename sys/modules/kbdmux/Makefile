# $MidnightBSD$
# $Id: Makefile,v 1.1 2005/06/23 00:30:50 max Exp $
# $FreeBSD: stable/10/sys/modules/kbdmux/Makefile 298430 2016-04-21 19:25:33Z emaste $

.PATH: ${.CURDIR}/../../dev/kbdmux

KMOD=	kbdmux
SRCS=	kbdmux.c opt_compat.h opt_kbd.h opt_kbdmux.h bus_if.h device_if.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
