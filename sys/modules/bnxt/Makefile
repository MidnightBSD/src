#$FreeBSD: stable/11/sys/modules/bnxt/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:  ${SRCTOP}/sys/dev/bnxt

KMOD    = if_bnxt
SRCS    = device_if.h bus_if.h pci_if.h pci_iov_if.h ifdi_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h
SRCS    += bnxt_txrx.c if_bnxt.c
SRCS	+= bnxt_hwrm.c bnxt_hwrm.h
SRCS	+= bnxt_sysctl.c bnxt_sysctl.h

CFLAGS+= -DIFLIB

.include <bsd.kmod.mk>
