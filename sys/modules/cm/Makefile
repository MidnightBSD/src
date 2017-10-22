# $FreeBSD: release/10.0.0/sys/modules/cm/Makefile 89099 2002-01-08 20:03:13Z fjoe $

.PATH: ${.CURDIR}/../../dev/cm

KMOD=	if_cm
SRCS=	smc90cx6.c if_cm_isa.c

SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>
