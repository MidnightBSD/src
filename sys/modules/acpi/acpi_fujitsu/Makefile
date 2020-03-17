# $FreeBSD: stable/11/sys/modules/acpi/acpi_fujitsu/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/acpi_support

KMOD=	acpi_fujitsu
CFLAGS+=-I${SRCTOP}/sys/dev/acpi_support
SRCS=	acpi_fujitsu.c opt_acpi.h acpi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
