# $FreeBSD: stable/11/sys/modules/dpt/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH:	${SYSDIR}/dev/dpt
KMOD=	dpt
SRCS=	dpt_scsi.c dpt.h \
	dpt_pci.c pci_if.h \
	opt_dpt.h opt_eisa.h \
	opt_cam.h opt_scsi.h \
	device_if.h bus_if.h

SRCS.DEV_EISA=	dpt_eisa.c eisa_if.h

.include <bsd.kmod.mk>
