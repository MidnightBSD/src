# $MidnightBSD$
# $FreeBSD: stable/10/cddl/lib/libavl/Makefile 321270 2017-07-20 01:01:16Z ngie $

.PATH: ${SRCTOP}/sys/cddl/contrib/opensolaris/common/avl

LIB=	avl
SRCS=	avl.c
WARNS?=	3
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
