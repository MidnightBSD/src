# $FreeBSD: stable/11/sys/modules/intelspi/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/intel
KMOD=	intelspi
SRCS=	spi.c
SRCS+=	acpi_if.h device_if.h bus_if.h opt_acpi.h spibus_if.h

.include <bsd.kmod.mk>
