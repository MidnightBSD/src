.PATH: ${SRCTOP}/sys/contrib/rdma/krping

KMOD= krping
SRCS= krping.c krping_dev.c getopt.c
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=  opt_sched.h opt_inet.h opt_inet6.h
CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>
