# $FreeBSD: stable/11/sys/modules/sdhci_acpi/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sdhci

KMOD=	sdhci_acpi
SRCS=	sdhci_acpi.c sdhci.h sdhci_if.h
SRCS+=	acpi_if.h device_if.h bus_if.h opt_acpi.h pci_if.h mmcbr_if.h

.include <bsd.kmod.mk>
