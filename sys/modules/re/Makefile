# $FreeBSD: stable/11/sys/modules/re/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/re

KMOD=	if_re
SRCS=	if_re.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
