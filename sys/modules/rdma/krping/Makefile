# $FreeBSD: stable/11/sys/modules/rdma/krping/Makefile 331769 2018-03-30 18:06:29Z hselasky $
.PATH: ${SRCTOP}/sys/contrib/rdma/krping

KMOD= krping
SRCS= krping.c krping_dev.c getopt.c
SRCS+=  bus_if.h device_if.h pci_if.h pcib_if.h vnode_if.h
SRCS+=  opt_sched.h opt_inet.h opt_inet6.h
CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>
