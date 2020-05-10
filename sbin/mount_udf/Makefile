# $FreeBSD: stable/11/sbin/mount_udf/Makefile 319185 2017-05-30 04:17:22Z ngie $

PACKAGE=runtime
PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=	mount_udf.8
LIBADD=	kiconv

MOUNT=	${.CURDIR:H}/mount
CFLAGS+= -I${MOUNT} -I${SRCTOP}/sys
.PATH:	${MOUNT}

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.include <bsd.prog.mk>
