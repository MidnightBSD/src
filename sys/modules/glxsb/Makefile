# $FreeBSD: stable/11/sys/modules/glxsb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/glxsb
KMOD	= glxsb
SRCS	= glxsb.c glxsb_hash.c glxsb.h 
SRCS   += device_if.h pci_if.h bus_if.h
SRCS   += opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
