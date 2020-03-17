# ioat Loadable Kernel Module
#
# $FreeBSD: stable/11/sys/modules/ioat/Makefile 319182 2017-05-30 04:11:12Z ngie $

IOAT_SRC_PATH = ${SRCTOP}/sys

.PATH:  ${IOAT_SRC_PATH}/dev/ioat

KMOD=	ioat
SRCS=	ioat.c ioat_test.c
SRCS+=	device_if.h bus_if.h pci_if.h
SRCS+=	opt_ddb.h

CFLAGS+=	-I${IOAT_SRC_PATH}

.include <bsd.kmod.mk>
