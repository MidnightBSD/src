# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/bce/Makefile 272001 2014-09-22 20:53:17Z davidcs $
.PATH:	${.CURDIR}/../../dev/bce
KMOD=	if_bce
SRCS=	opt_bce.h if_bce.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

#CFLAGS += -DBCE_DEBUG=0

.include <bsd.kmod.mk>
