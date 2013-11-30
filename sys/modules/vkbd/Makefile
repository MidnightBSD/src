# $Id: Makefile,v 1.1.1.2 2006-02-25 02:37:26 laffer1 Exp $
# $FreeBSD: src/sys/modules/vkbd/Makefile,v 1.2.2.1 2006/02/10 00:38:07 yar Exp $

.PATH:	${.CURDIR}/../../dev/vkbd

KMOD=	vkbd
SRCS=	vkbd.c opt_kbd.h

.if !defined(KERNBUILDDIR)
opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
