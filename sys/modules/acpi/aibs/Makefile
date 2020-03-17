# $FreeBSD: stable/11/sys/modules/acpi/aibs/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/acpi_support

KMOD=	aibs
SRCS=	atk0110.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h
SRCS+=	opt_ddb.h

.include <bsd.kmod.mk>
