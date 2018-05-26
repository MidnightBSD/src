# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/sound/driver/uaudio/Makefile 266485 2014-05-21 06:29:52Z hselasky $

S=	${.CURDIR}/../../../..

.PATH:	$S/dev/sound/usb

KMOD=	snd_uaudio
SRCS=	bus_if.h device_if.h usb_if.h vnode_if.h
SRCS+=	opt_usb.h opt_bus.h feeder_if.h channel_if.h usbdevs.h
SRCS+=	uaudio_pcm.c uaudio.c 

.include <bsd.kmod.mk>
