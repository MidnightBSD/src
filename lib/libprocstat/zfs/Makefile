# $MidnightBSD$
# $FreeBSD: stable/10/lib/libprocstat/zfs/Makefile 313612 2017-02-11 06:46:16Z ngie $

.PATH: ${.CURDIR:H}

SRCS=	zfs.c
OBJS=	zfs.o
WARNS?=	1

CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head
CFLAGS+= -I${.CURDIR:H}
CFLAGS+= -DNEED_SOLARIS_BOOLEAN

all: ${OBJS}
CLEANFILES= ${OBJS}

.include <bsd.lib.mk>
