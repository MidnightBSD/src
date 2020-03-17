# $FreeBSD: stable/11/sys/modules/pcfclock/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
