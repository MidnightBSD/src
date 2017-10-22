# $FreeBSD: release/10.0.0/sys/modules/usb/rsu/Makefile 253789 2013-07-30 02:07:57Z rpaulo $

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_rsu
SRCS    = if_rsu.c if_rsureg.h \
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
