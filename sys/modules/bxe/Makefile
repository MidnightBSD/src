# $FreeBSD: stable/9/sys/modules/bxe/Makefile 220603 2011-04-13 16:35:43Z yongari $
BXE	= ${.CURDIR}/../../dev/bxe
.PATH:	${BXE}

KMOD=	if_bxe
SRCS	= opt_bxe.h device_if.h bus_if.h pci_if.h
SRCS	+= if_bxe.c bxe_link.c

CFLAGS += -I${BXE} -DBXE_TASK
#CFLAGS += -DBXE_DEBUG
#CFLAGS += -DBXE_NVRAM_WRITE_SUPPORT

.include <bsd.kmod.mk>
