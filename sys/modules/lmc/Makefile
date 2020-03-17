# $FreeBSD: stable/11/sys/modules/lmc/Makefile 319182 2017-05-30 04:11:12Z ngie $

KMOD  = if_lmc
.PATH: ${SRCTOP}/sys/dev/lmc

SRCS  = if_lmc.c if_lmc.h
SRCS += device_if.h bus_if.h pci_if.h
SRCS += opt_inet.h opt_inet6.h
SRCS += opt_netgraph.h
SRCS += opt_bpf.h

opt_netgraph.h:
	echo "#define NETGRAPH 1" > ${.TARGET}

.include <bsd.kmod.mk>
