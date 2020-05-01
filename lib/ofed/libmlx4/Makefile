# $FreeBSD: stable/11/lib/ofed/libmlx4/Makefile 335253 2018-06-16 15:05:05Z kib $

_spath=	${SRCTOP}/contrib/ofed/libmlx4
.PATH: ${_spath}

SHLIBDIR?=	/usr/lib
LIB=		mlx4
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS= \
buf.c \
cq.c \
dbrec.c \
mlx4.c \
qp.c \
srq.c \
verbs.c

LIBADD=		ibverbs pthread
CFLAGS+=	-I${_spath}

.include <bsd.lib.mk>
