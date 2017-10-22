# $FreeBSD: release/7.0.0/sys/modules/sound/driver/uaudio/Makefile 139399 2004-12-29 08:50:35Z imp $

.PATH: ${.CURDIR}/../../../../dev/sound/usb

KMOD=	snd_uaudio
SRCS=	device_if.h bus_if.h opt_usb.h
SRCS+=	uaudio.c uaudio_pcm.c

.include <bsd.kmod.mk>
