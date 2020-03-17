# $FreeBSD: stable/11/sys/modules/ubsec/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/ubsec
KMOD	= ubsec
SRCS	= ubsec.c opt_ubsec.h
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bus.h cryptodev_if.h

.if !defined(KERNBUILDDIR)
opt_ubsec.h:
	echo "#define UBSEC_DEBUG 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
