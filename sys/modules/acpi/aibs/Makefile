# $FreeBSD: release/10.0.0/sys/modules/acpi/aibs/Makefile 209523 2010-06-25 15:32:46Z rpaulo $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	aibs
SRCS=	atk0110.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h
SRCS+=	opt_ddb.h

.include <bsd.kmod.mk>
