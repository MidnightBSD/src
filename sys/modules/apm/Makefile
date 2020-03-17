# $FreeBSD: stable/11/sys/modules/apm/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/i386/bios
.if ${MACHINE} == "pc98"
.PATH: ${.CURDIR}/../../pc98/apm
.endif

KMOD=	apm
SRCS=	apm.c apm.h
.if ${MACHINE} == "pc98"
SRCS+=	apm_bioscall.S
.endif
SRCS+=	device_if.h bus_if.h

EXPORT_SYMS=	apm_display	\
		apm_softc

.include <bsd.kmod.mk>
