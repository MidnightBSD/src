# $FreeBSD: release/10.0.0/sys/modules/acpi/acpi_asus_wmi/Makefile 237981 2012-07-02 08:31:29Z mav $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_asus_wmi
CFLAGS+=-I${.CURDIR}/../../../dev/acpi_support
SRCS=	acpi_asus_wmi.c opt_acpi.h acpi_if.h acpi_wmi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
