# $FreeBSD: stable/11/sys/modules/aic/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/aic

KMOD=	aic
SRCS=	aic.c aic_pccard.c
SRCS+=	device_if.h bus_if.h pci_if.h isa_if.h card_if.h pccarddevs.h
SRCS+=	opt_cam.h opt_scsi.h

.if ${MACHINE} == "pc98"
SRCS+=	aic_cbus.c
.else
SRCS+=	aic_isa.c
.endif

.include <bsd.kmod.mk>
