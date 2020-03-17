# $FreeBSD: stable/11/sys/modules/rccgpio/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/rccgpio
KMOD=	rccgpio
SRCS=	rccgpio.c
SRCS+=	device_if.h bus_if.h isa_if.h gpio_if.h opt_platform.h

.include <bsd.kmod.mk>
