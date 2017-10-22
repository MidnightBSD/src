# $FreeBSD: release/10.0.0/sys/modules/acpi/acpi_wmi/Makefile 194701 2009-06-23 13:17:25Z rpaulo $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_wmi
CFLAGS+=-I${.CURDIR}/../../../dev/acpi_support
SRCS=	acpi_wmi.c opt_acpi.h acpi_if.h acpi_wmi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
