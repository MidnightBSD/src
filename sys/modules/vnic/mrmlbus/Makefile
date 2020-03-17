# $FreeBSD: stable/11/sys/modules/vnic/mrmlbus/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

S=	${SRCTOP}/sys

.PATH: $S/dev/vnic

KMOD=	mrmlbus
SRCS=	device_if.h bus_if.h opt_platform.h pci_if.h ofw_bus_if.h miibus_if.h lmac_if.h
SRCS+=	mrml_bridge.c

CFLAGS+=	-DFDT

.include <bsd.kmod.mk>
