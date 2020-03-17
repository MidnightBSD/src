
# $FreeBSD: stable/11/sys/modules/aic7xxx/ahc/ahc_isa/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/aic7xxx
KMOD=		ahc_isa

SRCS=	ahc_isa.c
SRCS+=	device_if.h bus_if.h pci_if.h isa_if.h
SRCS+=	opt_scsi.h opt_cam.h opt_aic7xxx.h

CFLAGS+=	-I${SRCTOP}/sys/dev/aic7xxx -I..

.include <bsd.kmod.mk>
