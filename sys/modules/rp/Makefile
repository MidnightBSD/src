# $FreeBSD: stable/11/sys/modules/rp/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/rp

KMOD=	rp
SRCS=	rp.c rp_pci.c device_if.h bus_if.h pci_if.h opt_compat.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
