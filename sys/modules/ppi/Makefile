
# $FreeBSD: stable/11/sys/modules/ppi/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/ppbus
KMOD=	ppi
SRCS=	bus_if.h device_if.h ppbus_if.h \
	opt_ppb_1284.h \
	ppi.c ppb_1284.c

.include <bsd.kmod.mk>
