#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: stable/9/sbin/mount_ext2fs/Makefile 198236 2009-10-19 16:00:24Z ru $

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN=	mount_ext2fs.8

WARNS?=	2
MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}

.PATH:	${MOUNT}

.include <bsd.prog.mk>
