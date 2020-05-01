# $FreeBSD: stable/11/lib/ofed/libmlx5/Makefile 337681 2018-08-12 08:42:06Z kib $

_spath=	${SRCTOP}/contrib/ofed/libmlx5
.PATH: ${_spath}

SHLIBDIR?=	/lib
LIB=		mlx5
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS= \
buf.c \
cq.c \
dbrec.c \
mlx5.c \
qp.c \
srq.c \
verbs.c

LIBADD=		ibverbs pthread
CFLAGS+=	-I${_spath}
VERSION_MAP=	${_spath}/libmlx5.map

.include <bsd.lib.mk>

