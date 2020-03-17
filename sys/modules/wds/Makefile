# $FreeBSD: stable/11/sys/modules/wds/Makefile 270226 2014-08-20 16:09:05Z jhb $

.PATH: ${.CURDIR}/../../dev/wds

KMOD=	wds
SRCS=	wd7000.c
SRCS+=	bus_if.h device_if.h isa_if.h
SRCS+=	opt_cam.h

.include <bsd.kmod.mk>
