# $FreeBSD: stable/11/sys/modules/nand/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/nand

KMOD = nand
SRCS=	nand.c nand_bbt.c nand_cdev.c nand_generic.c nand_geom.c \
	nand_id.c nandbus.c nandbus_if.c nand_if.c nfc_if.c \
	nand_if.h device_if.h bus_if.h nfc_if.h nandbus_if.h

.include <bsd.kmod.mk>
