
# $FreeBSD: stable/9/sys/modules/aic7xxx/ahc/ahc_pci/Makefile 229273 2012-01-02 12:50:19Z dim $

.PATH:	${.CURDIR}/../../../../dev/aic7xxx
KMOD=	ahc_pci

../aic7xxx_reg.h:
	( cd .. ; ${MAKE} aic7xxx_reg.h )

SRCS=	ahc_pci.c aic7xxx_pci.c ../aic7xxx_reg.h
SRCS+=	device_if.h bus_if.h pci_if.h
SRCS+=	opt_scsi.h opt_cam.h opt_aic7xxx.h

CFLAGS+= -I${.CURDIR}/../../../../dev/aic7xxx -I..

.include <bsd.kmod.mk>

CWARNFLAGS.ahc_pci.c=	${NO_WCONSTANT_CONVERSION}
CWARNFLAGS+=		${CWARNFLAGS.${.IMPSRC:T}}
