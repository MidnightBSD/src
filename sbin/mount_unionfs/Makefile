#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: release/7.0.0/sbin/mount_unionfs/Makefile 166684 2007-02-13 05:39:38Z rodrigc $

PROG=	mount_unionfs
SRCS=	mount_unionfs.c getmntopts.c
MAN=	mount_unionfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	6

.PATH:	${MOUNT}

.include <bsd.prog.mk>
