# $FreeBSD: stable/11/sys/modules/usb/upgt/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/usb/wlan

KMOD    = if_upgt
SRCS    = if_upgt.c if_upgtvar.h	\
	  bus_if.h device_if.h \
	  opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
