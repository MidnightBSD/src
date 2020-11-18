# $FreeBSD: stable/11/sys/modules/mlxfw/Makefile 347285 2019-05-08 10:49:05Z hselasky $
.PATH:  ${SRCTOP}/sys/dev/mlxfw

KMOD=mlxfw
SRCS= \
mlxfw_fsm.c \
mlxfw_mfa2.c \
mlxfw_mfa2_tlv_multi.c
SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+= \
	-I${SRCTOP}/sys/ofed/include \
	-I${SRCTOP}/sys/compat/linuxkpi/common/include \
	-I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz

.include <bsd.kmod.mk>
