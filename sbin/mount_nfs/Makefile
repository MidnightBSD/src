#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: stable/11/sbin/mount_nfs/Makefile 319185 2017-05-30 04:17:22Z ngie $

PACKAGE=runtime
PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c mounttab.c
MAN=	mount_nfs.8

MOUNT=	${.CURDIR:H}/mount
UMNTALL= ${SRCTOP}/usr.sbin/rpc.umntall
CFLAGS+= -DNFS -I${MOUNT} -I${UMNTALL}

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
