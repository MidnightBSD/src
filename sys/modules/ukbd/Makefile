# $FreeBSD: release/7.0.0/sys/modules/ukbd/Makefile 162711 2006-09-27 19:57:02Z ru $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	ukbd
SRCS=	bus_if.h device_if.h opt_compat.h opt_usb.h opt_kbd.h opt_ukbd.h \
	usbdevs.h ukbd.c

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
