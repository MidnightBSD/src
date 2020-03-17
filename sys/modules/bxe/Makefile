# $FreeBSD: stable/11/sys/modules/bxe/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR	?= ${SRCTOP}/sys

BXE	= ${SYSDIR}/dev/bxe
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
