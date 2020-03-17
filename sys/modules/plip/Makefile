
# $FreeBSD: stable/11/sys/modules/plip/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/ppbus
KMOD=	plip
SRCS=	bus_if.h device_if.h ppbus_if.h \
	opt_plip.h \
	if_plip.c

.include <bsd.kmod.mk>
