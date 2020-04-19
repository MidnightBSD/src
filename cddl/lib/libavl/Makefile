# $FreeBSD: stable/11/cddl/lib/libavl/Makefile 321268 2017-07-20 00:53:50Z ngie $

.PATH: ${SRCTOP}/sys/cddl/contrib/opensolaris/common/avl

LIB=	avl
SRCS=	avl.c
WARNS?=	3
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
