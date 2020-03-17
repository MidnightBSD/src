# $FreeBSD: stable/11/sys/modules/i2c/controllers/lpbb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:		${SRCTOP}/sys/dev/ppbus
KMOD		= lpbb
SRCS		= device_if.h bus_if.h iicbb_if.h ppbus_if.h \
		  lpbb.c

.include <bsd.kmod.mk>
