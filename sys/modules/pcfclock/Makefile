# $FreeBSD: release/10.0.0/sys/modules/pcfclock/Makefile 70711 2001-01-06 14:00:42Z obrien $

.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
