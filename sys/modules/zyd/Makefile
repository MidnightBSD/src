# ZyDAS Loadable Kernel Module
#
# $FreeBSD: release/7.0.0/sys/modules/zyd/Makefile 172008 2007-08-29 21:04:26Z imp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD    = if_zyd
SRCS    = if_zyd.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
