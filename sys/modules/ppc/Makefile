# $FreeBSD: release/10.0.0/sys/modules/ppc/Makefile 211690 2010-08-23 06:13:29Z imp $

.PATH:	${.CURDIR}/../../dev/ppc
KMOD=	ppc
SRCS=	bus_if.h device_if.h ppbus_if.h isa_if.h pci_if.h serdev_if.h \
	opt_ppc.h opt_isa.h \
	ppc.c ppc_pci.c ppc_puc.c

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	ppc_acpi.c ppc_isa.c
.endif

.if ${MACHINE_CPUARCH} == "i386"
.if ${MACHINE} != "pc98"
SRCS+=	ppc_acpi.c
.endif
SRCS+=	ppc_isa.c
.endif

.if ${MACHINE_CPUARCH} == "ia64"
SRCS+=	ppc_acpi.c
.endif

.include <bsd.kmod.mk>
