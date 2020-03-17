# $FreeBSD: stable/11/sys/modules/isp/Makefile 331634 2018-03-27 17:46:25Z brooks $

.PATH: ${SRCTOP}/sys/dev/isp

KMOD=	isp
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_compat.h opt_ddb.h opt_isp.h \
	isp.c isp_library.c isp_target.c isp_freebsd.c isp_pci.c

.if ${MACHINE} == sparc64
SRCS+=	isp_sbus.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
