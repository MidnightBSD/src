# $MidnightBSD: src/sys/modules/cpufreq/Makefile,v 1.2 2007/03/29 22:44:53 laffer1 Exp $
# $FreeBSD: src/sys/modules/cpufreq/Makefile,v 1.8 2005/04/19 16:38:24 njl Exp $

.PATH:	${.CURDIR}/../../dev/cpufreq		\
	${.CURDIR}/../../${MACHINE_ARCH}/cpufreq

KMOD=	cpufreq
SRCS=	ichss.c
SRCS+=	bus_if.h cpufreq_if.h device_if.h pci_if.h

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
.PATH:	${.CURDIR}/../../i386/cpufreq
CFLAGS+= -I${.CURDIR}/../../contrib/dev/acpica

SRCS+=	acpi_if.h opt_acpi.h
SRCS+=	est.c hwpstate.c p4tcc.c powernow.c
.endif

.if ${MACHINE} == "i386"
SRCS+=	smist.c
.endif

.include <bsd.kmod.mk>
