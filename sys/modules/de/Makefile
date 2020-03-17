# $FreeBSD: stable/11/sys/modules/de/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/de

KMOD=	if_de
SRCS=	if_de.c opt_ddb.h device_if.h bus_if.h pci_if.h

# Compile-time options.

#  Enable debugging:
#CFLAGS+=	-DTULIP_DEBUG

#  Enable performance statistics:
#CFLAGS+=	-DTULIP_PERFSTATS

.include <bsd.kmod.mk>
