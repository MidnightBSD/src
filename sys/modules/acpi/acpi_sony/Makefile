# $FreeBSD: release/7.0.0/sys/modules/acpi/acpi_sony/Makefile 137715 2004-11-15 05:54:15Z imp $

.PATH:		${.CURDIR}/../../../dev/acpi_support

KMOD=		acpi_sony
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_sony.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
