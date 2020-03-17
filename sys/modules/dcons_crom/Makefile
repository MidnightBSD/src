# $Id: Makefile,v 1.6 2003/10/24 15:43:24 simokawa Exp $
# $FreeBSD: stable/11/sys/modules/dcons_crom/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/dcons

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h dcons_os.h \
		bus_if.h device_if.h

#KMODDEPS = firewire dcons

CFLAGS+= -I${SRCTOP}/sys

.include <bsd.kmod.mk>
