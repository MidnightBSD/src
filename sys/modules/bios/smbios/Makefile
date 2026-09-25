#

.PATH:  ${SRCTOP}/sys/dev/smbios

KMOD=	smbios
SRCS=	smbios.c \
	bus_if.h device_if.h

EXPORT_SYMS=	smbios_get_entry_point \
		smbios_get_structure_table

.include <bsd.kmod.mk>
