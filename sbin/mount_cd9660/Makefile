#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: stable/11/sbin/mount_cd9660/Makefile 319185 2017-05-30 04:17:22Z ngie $

PACKAGE=runtime
PROG=	mount_cd9660
SRCS=	mount_cd9660.c getmntopts.c
MAN=	mount_cd9660.8
LIBADD=	kiconv

MOUNT=	${.CURDIR:H}/mount
CFLAGS+= -I${MOUNT}

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.PATH:	${MOUNT}

.include <bsd.prog.mk>
