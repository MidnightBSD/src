# $FreeBSD: stable/9/sys/modules/ips/Makefile 245431 2013-01-14 19:51:26Z dim $

.PATH:		${.CURDIR}/../../dev/ips
KMOD		= ips
SRCS		= ips.c ips_pci.c ips.h ips_disk.c ips_disk.h ips_commands.c \
			ips_ioctl.h ips_ioctl.c device_if.h bus_if.h pci_if.h 

.include <bsd.kmod.mk>

# XXX Work around clang warning, until maintainer approves fix.
CWARNFLAGS.ips.c=	${NO_WSOMETIMES_UNINITIALIZED}
CWARNFLAGS+=		${CWARNFLAGS.${.IMPSRC:T}}
