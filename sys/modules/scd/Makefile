# $FreeBSD: release/10.0.0/sys/modules/scd/Makefile 106449 2002-11-05 09:37:32Z mdodd $

.PATH: ${.CURDIR}/../../dev/scd

KMOD=	scd
SRCS=	scd.c scd_isa.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>
