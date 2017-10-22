# $FreeBSD: release/10.0.0/sys/modules/x86bios/Makefile 211058 2010-08-08 06:18:05Z jkim $

KMOD=	x86bios
SRCS=	opt_x86bios.h
SRCS+=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../contrib/x86emu
.PATH:	${.CURDIR}/../../compat/x86bios
SRCS+=	x86bios.c
.if ${MACHINE_ARCH} != "i386"
SRCS+=	x86emu.c
.endif

.include <bsd.kmod.mk>
