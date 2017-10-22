# $FreeBSD: release/7.0.0/sys/modules/acpi/acpi_toshiba/Makefile 159066 2006-05-30 09:38:54Z ru $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_toshiba
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_toshiba.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
