# $FreeBSD: stable/11/sys/modules/bce/Makefile 319182 2017-05-30 04:11:12Z ngie $
.PATH:	${SRCTOP}/sys/dev/bce
KMOD=	if_bce
SRCS=	opt_bce.h if_bce.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

#CFLAGS += -DBCE_DEBUG=0

.include <bsd.kmod.mk>
