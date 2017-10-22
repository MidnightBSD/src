# $FreeBSD: release/10.0.0/sys/modules/ct/Makefile 240170 2012-09-06 18:02:32Z jhb $

.PATH: ${.CURDIR}/../../dev/ct

KMOD=	ct
SRCS=	bshw_machdep.c ct.c ct_isa.c
SRCS+=	device_if.h bus_if.h isa_if.h 
SRCS+=	opt_ct.h opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
