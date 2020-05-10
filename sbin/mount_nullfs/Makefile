#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: stable/11/sbin/mount_nullfs/Makefile 319185 2017-05-30 04:17:22Z ngie $

PACKAGE=runtime
PROG=	mount_nullfs
SRCS=	mount_nullfs.c getmntopts.c
MAN=	mount_nullfs.8

MOUNT=	${.CURDIR:H}/mount
CFLAGS+=-I${MOUNT}

.PATH:	${MOUNT}

.include <bsd.prog.mk>
