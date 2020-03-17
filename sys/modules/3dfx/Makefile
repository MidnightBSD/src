# $FreeBSD: stable/11/sys/modules/3dfx/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/tdfx

KMOD=	3dfx
SRCS=	bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
	tdfx_vars.h tdfx_pci.c

# Uncomment this for debugging messages
#CFLAGS+=	-DDEBUG

.include <bsd.kmod.mk>
