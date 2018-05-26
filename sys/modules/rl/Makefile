# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/rl/Makefile 292780 2015-12-27 17:12:54Z marius $

.PATH: ${.CURDIR}/../../dev/rl

KMOD=	if_rl
SRCS=	if_rl.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
