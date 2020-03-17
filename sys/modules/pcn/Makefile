# $FreeBSD: stable/11/sys/modules/pcn/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/pcn

KMOD=	if_pcn
SRCS=	if_pcn.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
