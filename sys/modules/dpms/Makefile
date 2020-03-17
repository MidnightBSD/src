# $FreeBSD: stable/11/sys/modules/dpms/Makefile 319182 2017-05-30 04:11:12Z ngie $

KMOD=	dpms
SRCS=	bus_if.h device_if.h pci_if.h

.PATH:	${SRCTOP}/sys/dev/dpms
SRCS+=	dpms.c

.include <bsd.kmod.mk>
