# $FreeBSD: stable/11/sys/modules/tpm/Makefile 350065 2019-07-16 19:23:47Z eugen $

.PATH:	${SRCTOP}/sys/dev/tpm

KMOD=	tpm

SRCS=	tpm.c bus_if.h device_if.h
#Bus specific stuff.
SRCS+=	tpm_isa.c tpm_acpi.c isa_if.h opt_acpi.h acpi_if.h
#TPM 2.0
SRCS+=	tpm20.c tpm_crb.c tpm_tis.c opt_tpm.h

.include <bsd.kmod.mk>
