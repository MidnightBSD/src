# $FreeBSD: stable/11/sys/modules/usb/urtw/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/usb/wlan

KMOD    = if_urtw
SRCS    = if_urtw.c if_urtwreg.h if_urtwvar.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
