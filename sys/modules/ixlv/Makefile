#$FreeBSD: stable/11/sys/modules/ixlv/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:  ${SRCTOP}/sys/dev/ixl

KMOD    = if_ixlv
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += opt_inet.h opt_inet6.h opt_rss.h opt_ixl.h
SRCS    += if_ixlv.c ixlvc.c ixl_txrx.c i40e_osdep.c

# Shared source
SRCS    += i40e_common.c i40e_nvm.c i40e_adminq.c

# Debug messages / sysctls
# CFLAGS += -DIXL_DEBUG

.include <bsd.kmod.mk>
