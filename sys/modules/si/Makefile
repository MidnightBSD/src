# $FreeBSD: stable/11/sys/modules/si/Makefile 310546 2016-12-25 17:01:58Z dim $

.PATH: ${.CURDIR}/../../dev/si

KMOD=	si
SRCS=	si.c si2_z280.c si3_t225.c si_isa.c si_pci.c
SRCS+=	bus_if.h device_if.h isa_if.h pci_if.h
SRCS+=	opt_compat.h opt_debug_si.h opt_eisa.h
SRCS.DEV_EISA=si_eisa.c eisa_if.h

.include <bsd.kmod.mk>

CWARNFLAGS.si.c=	${NO_WCONSTANT_CONVERSION}
