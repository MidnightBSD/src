# $MidnightBSD$
# Makefile for mrsas driver
# $FreeBSD: stable/10/sys/modules/mrsas/mrsas_linux/Makefile 275976 2014-12-21 02:35:22Z smh $

.PATH: ${.CURDIR}/../../../dev/mrsas

KMOD=	mrsas_linux
SRCS=	mrsas_linux.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
