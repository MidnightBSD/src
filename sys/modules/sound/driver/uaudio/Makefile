# $FreeBSD: release/10.0.0/sys/modules/sound/driver/uaudio/Makefile 188957 2009-02-23 21:19:18Z thompsa $

S=	${.CURDIR}/../../../..

.PATH:	$S/dev/sound/usb

KMOD=	snd_uaudio
SRCS=	bus_if.h device_if.h usb_if.h vnode_if.h
SRCS+=	opt_usb.h opt_bus.h feeder_if.h channel_if.h usbdevs.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>
