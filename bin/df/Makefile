#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: stable/11/bin/df/Makefile 319184 2017-05-30 04:15:40Z ngie $

PACKAGE=runtime
MOUNT=	${SRCTOP}/sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

CFLAGS+= -DMOUNT_CHAR_DEVS
SRCS+=	getmntopts.c

LIBADD=	xo util

.include <bsd.prog.mk>
