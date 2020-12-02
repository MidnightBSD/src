# $FreeBSD: stable/11/sys/modules/iser/Makefile 331769 2018-03-30 18:06:29Z hselasky $

.PATH:	${SRCTOP}/sys/dev/iser/

.include <bsd.own.mk>

KMOD=	iser

SRCS=	icl_iser.c
SRCS+=	iser_initiator.c
SRCS+=	iser_memory.c
SRCS+=	iser_verbs.c

SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_inet.h opt_inet6.h opt_cam.h icl_conn_if.h

CFLAGS+= -I${SRCTOP}/sys/
CFLAGS+= -I${SYSDIR}/ofed/include
CFLAGS+= -I${SYSDIR}/ofed/include/uapi
CFLAGS+= -I${SYSDIR}/compat/linuxkpi/common/include
CFLAGS+= -DCONFIG_INFINIBAND_USER_MEM
CFLAGS+= -DINET6 -DINET
CFLAGS+= -fms-extensions

CFLAGS+=-DICL_KERNEL_PROXY

MFILES=	kern/bus_if.m kern/device_if.m dev/iscsi/icl_conn_if.m

.include <bsd.kmod.mk>
