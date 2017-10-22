#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: release/10.0.0/sbin/mount_nfs/Makefile 221288 2011-05-01 01:34:22Z rmacklem $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c mounttab.c
MAN=	mount_nfs.8
MLINKS=	mount_nfs.8 mount_oldnfs.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -DNFS -I${MOUNT} -I${UMNTALL}
WARNS?=	3

LINKS=	${BINDIR}/mount_nfs ${BINDIR}/mount_oldnfs

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
