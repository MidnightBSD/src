# $Id: Makefile,v 1.1.1.1 2006-02-25 02:26:21 laffer1 Exp $

PROG=	mount_smbfs
SRCS=	mount_smbfs.c getmntopts.c
MAN8=	mount_smbfs.8

BINDIR=	/sbin
#NOSHARED=yes

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DSMBFS -I${MOUNT}

.PATH:	${MOUNT}

LDADD+= -lsmb
DPADD+= ${LIBSMB}

.include <bsd.prog.mk>
