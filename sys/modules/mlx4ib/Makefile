# $FreeBSD: stable/11/sys/modules/mlx4ib/Makefile 331769 2018-03-30 18:06:29Z hselasky $
.PATH:  ${SRCTOP}/sys/dev/mlx4/mlx4_ib

KMOD=	mlx4ib
SRCS= \
	mlx4_ib_alias_GUID.c \
	mlx4_ib_mcg.c \
	mlx4_ib_sysfs.c \
	mlx4_ib_ah.c \
	mlx4_ib_cq.c \
	mlx4_ib_doorbell.c \
	mlx4_ib_mad.c \
	mlx4_ib_main.c \
	mlx4_ib_mr.c \
	mlx4_ib_qp.c \
	mlx4_ib_srq.c \
	mlx4_ib_wc.c \
	mlx4_ib_cm.c
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_inet.h opt_inet6.h

CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+= -DCONFIG_INFINIBAND_USER_MEM
CFLAGS+= -DINET6 -DINET

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith
