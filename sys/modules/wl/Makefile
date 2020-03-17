# $FreeBSD: stable/11/sys/modules/wl/Makefile 270226 2014-08-20 16:09:05Z jhb $

.PATH: ${.CURDIR}/../../dev/wl

KMOD=	if_wl
SRCS=	if_wl.c
SRCS+=	bus_if.h device_if.h isa_if.h
SRCS+=	opt_inet.h opt_wavelan.h

.include <bsd.kmod.mk>
