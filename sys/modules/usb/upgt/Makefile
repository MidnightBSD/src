# $FreeBSD: release/10.0.0/sys/modules/usb/upgt/Makefile 191983 2009-05-11 02:39:49Z weongyo $

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_upgt
SRCS    = if_upgt.c if_upgtvar.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
