# $FreeBSD: stable/11/usr.bin/ofed/libibverbs/Makefile.inc 335253 2018-06-16 15:05:05Z kib $

_spath=${SRCTOP}/contrib/ofed/libibverbs
.PATH: ${_spath}/examples ${_spath}/man

BINDIR?=	/usr/bin
CFLAGS+=	-I${_spath}
LIBADD+=	ibverbs mlx4 mlx5 cxgb4 pthread

