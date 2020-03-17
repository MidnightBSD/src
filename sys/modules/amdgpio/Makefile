# $FreeBSD: stable/11/sys/modules/amdgpio/Makefile 339523 2018-10-21 04:52:37Z gonzo $

.PATH: ${SRCTOP}/sys/dev/amdgpio
KMOD=  amdgpio
SRCS=  amdgpio.c
SRCS+= acpi_if.h device_if.h bus_if.h gpio_if.h pic_if.h opt_acpi.h opt_platform.h

.include <bsd.kmod.mk>
