# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/netfpga10g/nf10bmac/Makefile 264601 2014-04-17 12:33:26Z bz $

.PATH: ${.CURDIR}/../../../dev/netfpga10g/nf10bmac

KMOD=	if_nf10bmac
SRCS=	if_nf10bmac.c
SRCS+=	device_if.h bus_if.h pci_if.h
SRCS+=	opt_device_polling.h

.if ${MACHINE_CPUARCH} == "mips"
SRCS+=	if_nf10bmac_fdt.c ofw_bus_if.h
.endif

CFLAGS+=	-DDEVICE_POLLING

.include <bsd.kmod.mk>
