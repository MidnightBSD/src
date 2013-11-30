# $Id: Makefile,v 1.2 2007-01-07 22:36:50 laffer1 Exp $
# $FreeBSD: src/sys/modules/kbdmux/Makefile,v 1.1.2.3 2006/03/04 00:08:20 emax Exp $

.PATH: ${.CURDIR}/../../dev/kbdmux

KMOD=	kbdmux
SRCS=	kbdmux.c opt_kbd.h bus_if.h device_if.h

.if !defined(KERNBUILDDIR)
opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
