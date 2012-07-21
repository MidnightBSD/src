# $MidnightBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.2 2007/03/17 23:17:24 archite Exp $
# $FreeBSD: src/sys/modules/sound/driver/uaudio/Makefile,v 1.3 2004/12/29 08:50:35 imp Exp $

S=	${.CURDIR}/../../../..

.PATH:	$S/dev/sound/usb

KMOD=	snd_uaudio
SRCS=	bus_if.h device_if.h usb_if.h vnode_if.h
SRCS+=	opt_usb.h opt_bus.h feeder_if.h channel_if.h usbdevs.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>
