#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: release/7.0.0/sbin/mount_nullfs/Makefile 152670 2005-11-21 22:51:16Z rodrigc $

PROG=	mount_nullfs
SRCS=	mount_nullfs.c getmntopts.c
MAN=	mount_nullfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	6

.PATH:	${MOUNT}

.include <bsd.prog.mk>
