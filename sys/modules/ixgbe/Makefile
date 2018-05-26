# $MidnightBSD$
#$FreeBSD: stable/10/sys/modules/ixgbe/Makefile 283668 2015-05-28 20:56:11Z erj $

.PATH:  ${.CURDIR}/../../dev/ixgbe

KMOD    = if_ixgbe
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += ixgbe.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe -DSMP

.include <bsd.kmod.mk>
