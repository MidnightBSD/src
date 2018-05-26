# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/bxe/Makefile 271988 2014-09-22 19:00:30Z davidcs $
BXE	= ${.CURDIR}/../../dev/bxe
.PATH:	${BXE}

KMOD	= if_bxe
SRCS	= device_if.h bus_if.h pci_if.h
SRCS	+= bxe.c               \
           bxe_stats.c         \
           bxe_debug.c         \
           bxe_elink.c         \
           ecore_sp.c          \
           57710_init_values.c \
           57711_init_values.c \
           57712_init_values.c

CFLAGS += -I${BXE}

.include <bsd.kmod.mk>
