# $FreeBSD: stable/9/sys/modules/3dfx/Makefile 242112 2012-10-26 00:44:32Z eadler $

.PATH: $(.CURDIR)/../../dev/tdfx

KMOD=	3dfx
SRCS=	bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
	tdfx_vars.h tdfx_pci.c
INCSRC=	../../sys

# Uncomment this for debugging messages
#CFLAGS+=	-DDEBUG

.include <bsd.kmod.mk>
