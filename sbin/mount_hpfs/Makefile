# $MidnightBSD$
# $FreeBSD: src/sbin/mount_hpfs/Makefile,v 1.6 2004/02/23 20:13:53 johan Exp $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN=	mount_hpfs.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT} -I${.CURDIR}/../../ -DHPFS
WARNS?=	6

.PATH:	${MOUNT}

.include <bsd.prog.mk>
