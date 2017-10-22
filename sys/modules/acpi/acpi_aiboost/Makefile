# $FreeBSD: release/7.0.0/sys/modules/acpi/acpi_aiboost/Makefile 163778 2006-10-30 05:38:48Z takawata $

.PATH:		${.CURDIR}/../../../dev/acpi_support

KMOD=		acpi_aiboost
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_aiboost.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
