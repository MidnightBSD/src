# $FreeBSD: stable/11/sys/modules/hatm/Makefile 319182 2017-05-30 04:11:12Z ngie $
#
# Author: Harti Brandt <harti@freebsd.org>
#
.PATH: ${SRCTOP}/sys/dev/hatm

KMOD=	if_hatm
SRCS=	if_hatm.c if_hatm_intr.c if_hatm_ioctl.c if_hatm_tx.c if_hatm_rx.c \
	device_if.h bus_if.h pci_if.h opt_inet.h opt_natm.h

CFLAGS+= -DENABLE_BPF
# CFLAGS+= -DHATM_DEBUG -DINVARIANT_SUPPORT -DINVARIANTS

.include <bsd.kmod.mk>
