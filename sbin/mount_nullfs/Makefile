#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: release/10.0.0/sbin/mount_nullfs/Makefile 198236 2009-10-19 16:00:24Z ru $

PROG=	mount_nullfs
SRCS=	mount_nullfs.c getmntopts.c
MAN=	mount_nullfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}

.PATH:	${MOUNT}

.include <bsd.prog.mk>
