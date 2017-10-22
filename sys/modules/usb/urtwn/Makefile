# $FreeBSD: release/10.0.0/sys/modules/usb/urtwn/Makefile 251538 2013-06-08 16:02:31Z rpaulo $

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_urtwn
SRCS    = if_urtwn.c if_urtwnreg.h \
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
