# $FreeBSD: stable/11/sys/modules/bytgpio/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/gpio
KMOD=	bytgpio
SRCS=	bytgpio.c
SRCS+=	acpi_if.h device_if.h bus_if.h gpio_if.h opt_acpi.h opt_platform.h

.include <bsd.kmod.mk>
