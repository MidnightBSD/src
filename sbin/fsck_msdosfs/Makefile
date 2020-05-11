#	$NetBSD: Makefile,v 1.6 1997/05/08 21:11:11 gwr Exp $
# $FreeBSD: stable/11/sbin/fsck_msdosfs/Makefile 360490 2020-04-30 06:34:34Z delphij $

FSCK=	${.CURDIR:H}/fsck
.PATH: ${FSCK}

PACKAGE=runtime
PROG=	fsck_msdosfs
MAN=	fsck_msdosfs.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c

CFLAGS+= -I${FSCK} -DHAVE_LIBUTIL_H
LIBADD=	util

.include <bsd.prog.mk>
