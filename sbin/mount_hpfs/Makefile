#
# $FreeBSD: stable/9/sbin/mount_hpfs/Makefile 204403 2010-02-27 10:14:59Z uqs $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN=	mount_hpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT} -DHPFS

.PATH:	${MOUNT}

.include <bsd.prog.mk>
