# $FreeBSD: stable/11/sys/modules/sound/driver/davbus/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sound/macio

KMOD=	snd_davbus
SRCS=	device_if.h bus_if.h ofw_bus_if.h
SRCS+=	channel_if.h feeder_if.h mixer_if.h
SRCS+=	aoa.c davbus.c

.include <bsd.kmod.mk>
