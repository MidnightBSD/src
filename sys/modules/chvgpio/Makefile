# $FreeBSD: stable/11/sys/modules/chvgpio/Makefile 329832 2018-02-22 19:12:32Z gonzo $

.PATH:	${SRCTOP}/sys/dev/gpio
KMOD=	chvgpio
SRCS=	chvgpio.c
SRCS+=	acpi_if.h device_if.h bus_if.h gpio_if.h opt_acpi.h opt_platform.h

.include <bsd.kmod.mk>
