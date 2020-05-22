# $FreeBSD: stable/11/usr.bin/ofed/librdmacm/Makefile.inc 335253 2018-06-16 15:05:05Z kib $

_spath=${SRCTOP}/contrib/ofed/librdmacm
.PATH: ${_spath}/examples ${_spath}/man

BINDIR?=	/usr/bin
CFLAGS+=	-I${SRCTOP}/contrib/ofed
LIBADD+=	ibverbs rdmacm mlx4 mlx5 cxgb4 pthread
