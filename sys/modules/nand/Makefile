# $FreeBSD: release/10.0.0/sys/modules/nand/Makefile 237605 2012-06-26 18:08:03Z takawata $

.PATH: ${.CURDIR}/../../dev/nand

KMOD = nand
SRCS=	nand.c nand_bbt.c nand_cdev.c nand_generic.c nand_geom.c \
	nand_id.c nandbus.c nandbus_if.c nand_if.c nfc_if.c \
	nand_if.h device_if.h bus_if.h nfc_if.h nandbus_if.h

MFILES=	kern/bus_if.m kern/device_if.m dev/nand/nfc_if.m \
	dev/nand/nand_if.m dev/nand/nandbus_if.m

.include <bsd.kmod.mk>
