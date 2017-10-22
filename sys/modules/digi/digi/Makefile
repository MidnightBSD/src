# $FreeBSD: release/10.0.0/sys/modules/digi/digi/Makefile 182668 2008-09-01 23:59:00Z imp $

.PATH:	${.CURDIR}/../../../dev/digi
KMOD=	digi
SRCS=	digi.c digi_pci.c digi_isa.c
SRCS+=	digi.h digi_pci.h digireg.h digi_mod.h
SRCS+=	bus_if.h pci_if.h device_if.h
SRCS+=	opt_compat.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > ${.TARGET}
	echo "#define COMPAT_FREEBSD6 1" >> ${.TARGET}
.endif

.include <bsd.kmod.mk>
