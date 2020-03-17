# $FreeBSD: stable/11/sys/modules/tsec/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys

.PATH:	${SYSDIR}/dev/tsec

KMOD=	if_tsec
SRCS=	if_tsec.c if_tsec_fdt.c
SRCS+=	bus_if.h device_if.h miibus_if.h miidevs.h ofw_bus_if.h

.include <bsd.kmod.mk>
