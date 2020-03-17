# $FreeBSD: stable/11/sys/modules/firewire/fwe/Makefile 319182 2017-05-30 04:11:12Z ngie $

# Makefile for the fwe(4) module (Ethernet over IEEE1394)

.PATH: ${SRCTOP}/sys/dev/firewire

KMOD	= if_fwe
SRCS	= bus_if.h device_if.h \
	  opt_inet.h \
	  if_fwe.c if_fwevar.h \
	  firewire.h firewirereg.h
#CFLAGS += -DDEVICE_POLLING

.include <bsd.kmod.mk>
