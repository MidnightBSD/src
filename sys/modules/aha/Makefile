# $MidnightBSD: src/sys/modules/aha/Makefile,v 1.2 2009/03/15 14:13:04 laffer1 Exp $
# $FreeBSD: src/sys/modules/aha/Makefile,v 1.8.14.1 2006/02/10 00:37:58 yar Exp $

.PATH: ${.CURDIR}/../../dev/aha

KMOD=	aha
SRCS=	aha.c aha_isa.c ahareg.h opt_cam.h device_if.h bus_if.h \
	isa_if.h

.include <bsd.kmod.mk>
