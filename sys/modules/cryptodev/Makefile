# $FreeBSD: stable/11/sys/modules/cryptodev/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/opencrypto
KMOD	= cryptodev
SRCS	= cryptodev.c
SRCS	+= bus_if.h device_if.h opt_compat.h

.include <bsd.kmod.mk>
