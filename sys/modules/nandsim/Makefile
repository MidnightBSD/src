# $FreeBSD: release/10.0.0/sys/modules/nandsim/Makefile 235537 2012-05-17 10:11:18Z gber $

.PATH: ${.CURDIR}/../../dev/nand

KMOD=	nandsim
SRCS=	nandsim.c nandsim_chip.c nandsim_swap.c nandsim_ctrl.c nandsim_log.c\
	bus_if.h device_if.h vnode_if.h nfc_if.h nand_if.h
MFILES=	kern/bus_if.m kern/device_if.m\
	dev/nand/nfc_if.m dev/nand/nand_if.m

.include <bsd.kmod.mk>
