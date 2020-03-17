# $FreeBSD: stable/11/sys/modules/evdev/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/evdev

KMOD=	evdev
SRCS=	cdev.c evdev.c evdev_mt.c evdev_utils.c
SRCS+=	opt_evdev.h bus_if.h device_if.h

.include <bsd.kmod.mk>
