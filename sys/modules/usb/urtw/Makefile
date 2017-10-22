# $FreeBSD: release/10.0.0/sys/modules/usb/urtw/Makefile 192873 2009-05-27 03:57:38Z weongyo $

.PATH: ${.CURDIR}/../../../dev/usb/wlan

KMOD    = if_urtw
SRCS    = if_urtw.c if_urtwreg.h if_urtwvar.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
