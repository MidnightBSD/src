# $FreeBSD: release/10.0.0/sys/modules/sk/Makefile 158061 2006-04-27 00:14:02Z yongari $

.PATH: ${.CURDIR}/../../dev/sk

KMOD=	if_sk
SRCS=	if_sk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
