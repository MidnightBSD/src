# $FreeBSD: release/10.0.0/sys/modules/cardbus/Makefile 153811 2005-12-29 01:43:47Z imp $

.PATH: ${.CURDIR}/../../dev/cardbus

KMOD=	cardbus
SRCS=	cardbus.c cardbus_cis.c cardbus_device.c \
	device_if.h bus_if.h card_if.h power_if.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>
