# $FreeBSD: stable/11/sys/modules/mpt/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mpt

KMOD=	mpt
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_ddb.h \
	mpt.c mpt_cam.c mpt_debug.c mpt_pci.c mpt_raid.c mpt_user.c

.include <bsd.kmod.mk>
