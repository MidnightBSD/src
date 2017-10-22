# $FreeBSD: release/10.0.0/sys/modules/padlock/Makefile 167771 2007-03-21 17:37:13Z sam $

.PATH: ${.CURDIR}/../../crypto/via

KMOD=	padlock
SRCS=	padlock.c padlock_cipher.c padlock_hash.c
SRCS   += device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
