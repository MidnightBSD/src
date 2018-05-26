# $MidnightBSD$
# ioat Loadable Kernel Module
#
# $FreeBSD: stable/10/sys/modules/ioat/Makefile 300661 2016-05-25 07:09:54Z mav $

IOAT_SRC_PATH = ${.CURDIR}/../..

.PATH:  ${IOAT_SRC_PATH}/dev/ioat

KMOD=	ioat
SRCS=	ioat.c ioat_test.c
SRCS+=	device_if.h bus_if.h pci_if.h

CFLAGS+=	-I${IOAT_SRC_PATH}

.include <bsd.kmod.mk>
