# $FreeBSD: stable/11/sys/modules/padlock_rng/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/random

KMOD=	padlock_rng
SRCS=	nehemiah.c
SRCS+=	bus_if.h device_if.h

CFLAGS+= -I${SRCTOP}/sys

.include <bsd.kmod.mk>
