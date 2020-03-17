# $FreeBSD: stable/11/sys/modules/fdc/Makefile 338544 2018-09-09 18:08:38Z eugen $

KMOD=	fdc

.PATH:  ${SRCTOP}/sys/dev/fdc
.if ${MACHINE} == "pc98"
SRCS=	fdc.c fdc_cbus.c
.else
SRCS=	fdc.c fdc_isa.c fdc_pccard.c
.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
SRCS+=	opt_acpi.h acpi_if.h fdc_acpi.c
.endif
.endif

SRCS+=	opt_fdc.h bus_if.h card_if.h device_if.h \
	isa_if.h pccarddevs.h

.include <bsd.kmod.mk>
