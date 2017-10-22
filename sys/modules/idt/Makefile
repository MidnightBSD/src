# $FreeBSD: release/7.0.0/sys/modules/idt/Makefile 104173 2002-09-30 05:12:39Z mdodd $

.PATH:	${.CURDIR}/../../dev/idt
  
KMOD=	idt
SRCS=	idt_pci.c idt_harp.c idt.c
SRCS+=	pci_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
