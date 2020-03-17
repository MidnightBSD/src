# $FreeBSD: stable/11/sys/modules/tx/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/tx

KMOD=	if_tx
SRCS=	if_tx.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h

.include <bsd.kmod.mk>
