# $FreeBSD: release/7.0.0/sys/modules/acpi/Makefile 167814 2007-03-22 18:16:43Z jkim $

.if ${MACHINE} == "i386"
SUBDIR=		acpi
.endif

SUBDIR+=	acpi_aiboost acpi_asus acpi_fujitsu acpi_ibm		\
		acpi_panasonic acpi_sony acpi_toshiba acpi_video	\
		acpi_dock

.include <bsd.subdir.mk>
