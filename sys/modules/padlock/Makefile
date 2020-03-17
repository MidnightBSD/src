# $FreeBSD: stable/11/sys/modules/padlock/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/crypto/via

KMOD=	padlock
SRCS=	padlock.c padlock_cipher.c padlock_hash.c
SRCS   += device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
