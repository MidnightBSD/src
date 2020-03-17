# $FreeBSD: stable/11/sys/modules/siba_bwn/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/siba

KMOD=	siba_bwn
SRCS=	siba_core.c siba_bwn.c sibareg.h sibavar.h
SRCS+=	device_if.h bus_if.h pci_if.h opt_siba.h

.include <bsd.kmod.mk>
