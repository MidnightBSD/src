# $FreeBSD: release/10.0.0/sys/modules/usb/uath/Makefile 190688 2009-04-04 11:23:00Z weongyo $

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_uath
SRCS    = if_uath.c if_uathvar.h if_uathreg.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
