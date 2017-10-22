# $FreeBSD: release/10.0.0/sys/modules/cryptodev/Makefile 213781 2010-10-13 14:44:38Z rpaulo $

.PATH:	${.CURDIR}/../../opencrypto
KMOD	= cryptodev
SRCS	= cryptodev.c
SRCS	+= bus_if.h device_if.h opt_compat.h

.include <bsd.kmod.mk>
