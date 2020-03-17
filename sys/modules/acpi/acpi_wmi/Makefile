# $FreeBSD: stable/11/sys/modules/acpi/acpi_wmi/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/acpi_support

KMOD=	acpi_wmi
CFLAGS+=-I${SRCTOP}/sys/dev/acpi_support
SRCS=	acpi_wmi.c opt_acpi.h acpi_if.h acpi_wmi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
