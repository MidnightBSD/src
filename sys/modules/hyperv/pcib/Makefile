# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/hyperv/pcib/Makefile 309313 2016-11-30 06:26:37Z dexuan $

.PATH:  ${.CURDIR}/../../../dev/hyperv/pcib \
	${.CURDIR}/../../../dev/hyperv/vmbus

KMOD=	vmbus_pcib
SRCS=	vmbus_pcib.c
SRCS+=	bus_if.h device_if.h pci_if.h pcib_if.h vmbus_if.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/pcib

.include <bsd.kmod.mk>
