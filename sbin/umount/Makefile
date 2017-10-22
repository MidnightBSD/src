#	@(#)Makefile	8.4 (Berkeley) 6/22/95
#
# $FreeBSD: release/7.0.0/sbin/umount/Makefile 126178 2004-02-23 20:25:27Z johan $

PROG=	umount
SRCS=	umount.c vfslist.c mounttab.c
WARNS?=	0
MAN=	umount.8

MOUNT=	${.CURDIR}/../mount
UMNTALL= ${.CURDIR}/../../usr.sbin/rpc.umntall
CFLAGS+= -I${MOUNT} -I${UMNTALL}

.PATH: ${MOUNT} ${UMNTALL}

.include <bsd.prog.mk>
