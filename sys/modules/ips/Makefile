# $FreeBSD: release/10.0.0/sys/modules/ips/Makefile 239462 2012-08-20 18:33:03Z dim $

.PATH:		${.CURDIR}/../../dev/ips
KMOD		= ips
SRCS		= ips.c ips_pci.c ips.h ips_disk.c ips_disk.h ips_commands.c \
			ips_ioctl.h ips_ioctl.c device_if.h bus_if.h pci_if.h 

.include <bsd.kmod.mk>

# XXX Work around clang warning, until maintainer approves fix.
CWARNFLAGS.ips.c=	${NO_WSOMETIMES_UNINITIALIZED}
CWARNFLAGS+=		${CWARNFLAGS.${.IMPSRC:T}}
