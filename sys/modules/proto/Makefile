# $FreeBSD: stable/11/sys/modules/proto/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/proto

KMOD=	proto
SRCS=	\
	proto_bus_isa.c \
	proto_bus_pci.c \
	proto_busdma.c \
	proto_core.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	isa_if.h \
	pci_if.h

.include <bsd.kmod.mk>
