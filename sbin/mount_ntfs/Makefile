#
# $FreeBSD: stable/9/sbin/mount_ntfs/Makefile 204404 2010-02-27 10:16:14Z uqs $
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN=	mount_ntfs.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.PATH:	${MOUNT}

.include <bsd.prog.mk>
