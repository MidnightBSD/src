# $Id: Makefile,v 1.1.1.1 2006-02-25 02:28:24 laffer1 Exp $
# $FreeBSD: src/sys/modules/dcons_crom/Makefile,v 1.2 2004/10/13 05:41:09 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h dcons_os.h \
		bus_if.h device_if.h

#KMODDEPS = firewire dcons

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
