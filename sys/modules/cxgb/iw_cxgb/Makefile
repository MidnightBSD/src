# $FreeBSD: stable/11/sys/modules/cxgb/iw_cxgb/Makefile 319182 2017-05-30 04:11:12Z ngie $

CXGB = ${SRCTOP}/sys/dev/cxgb
.PATH: ${CXGB}/ulp/iw_cxgb

KMOD=   iw_cxgb
SRCS=   iw_cxgb.c iw_cxgb_cm.c iw_cxgb_hal.c 
SRCS+=  iw_cxgb_provider.c iw_cxgb_qp.c iw_cxgb_resource.c
SRCS+=  iw_cxgb_ev.c iw_cxgb_mem.c iw_cxgb_dbg.c iw_cxgb_cq.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h opt_ktr.h
SRCS+=  opt_inet.h opt_ofed.h vnode_if.h
CFLAGS+= -I${CXGB} -I${SRCTOP}/sys/ofed/include -DLINUX_TYPES_DEFINED
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>
