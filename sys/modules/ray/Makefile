# $FreeBSD: release/7.0.0/sys/modules/ray/Makefile 129740 2004-05-26 00:53:10Z imp $

.PATH: ${.CURDIR}/../../dev/ray

KMOD=	if_ray
SRCS=	if_ray.c
SRCS+=	device_if.h bus_if.h card_if.h opt_inet.h pccarddevs.h

.include <bsd.kmod.mk>
