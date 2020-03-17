# Makefile for mrsas driver
# $FreeBSD: stable/11/sys/modules/mrsas/mrsas_linux/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mrsas

KMOD=	mrsas_linux
SRCS=	mrsas_linux.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
