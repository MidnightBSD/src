# $FreeBSD: stable/11/sys/modules/mse/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mse

KMOD=	mse
SRCS=	mse.c device_if.h bus_if.h isa_if.h

.if (${MACHINE} == "pc98")
SRCS+= mse_cbus.c
.else
SRCS+= mse_isa.c
.endif

.include <bsd.kmod.mk>
