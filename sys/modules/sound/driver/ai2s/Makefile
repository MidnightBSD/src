# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/sound/driver/ai2s/Makefile 264375 2014-04-12 19:57:15Z andreast $

.PATH: ${.CURDIR}/../../../../dev/sound/macio

KMOD=	snd_ai2s
SRCS=	device_if.h bus_if.h ofw_bus_if.h
SRCS+=	channel_if.h feeder_if.h mixer_if.h
SRCS+=	onyx.c snapper.c tumbler.c aoa.c i2s.c

.include <bsd.kmod.mk>
