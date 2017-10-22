#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: release/7.0.0/sbin/mount_ext2fs/Makefile 122622 2003-11-13 19:18:43Z johan $

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN=	mount_ext2fs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}

.PATH:	${MOUNT}

.include <bsd.prog.mk>
