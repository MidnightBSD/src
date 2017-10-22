#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: release/10.0.0/sbin/mount_unionfs/Makefile 198236 2009-10-19 16:00:24Z ru $

PROG=	mount_unionfs
SRCS=	mount_unionfs.c getmntopts.c
MAN=	mount_unionfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}

.PATH:	${MOUNT}

.include <bsd.prog.mk>
