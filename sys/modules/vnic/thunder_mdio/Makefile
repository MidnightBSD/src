# $FreeBSD: stable/11/sys/modules/vnic/thunder_mdio/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

S=	${SRCTOP}/sys

.PATH: $S/dev/vnic

KMOD=	thunder_mdio
SRCS=	opt_platform.h device_if.h bus_if.h pci_if.h ofw_bus_if.h miibus_if.h lmac_if.h
SRCS+=	thunder_mdio.c thunder_mdio_fdt.c

CFLAGS+=	-DFDT

.include <bsd.kmod.mk>
