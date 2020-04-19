#	@(#)Makefile	8.6 (Berkeley) 5/8/95
# $FreeBSD: stable/11/sbin/mount/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=runtime
PROG=	mount
SRCS=	mount.c mount_fs.c getmntopts.c vfslist.c
MAN=	mount.8 mount.conf.8
# We do NOT install the getmntopts.3 man page.

LIBADD=	util

.include <bsd.prog.mk>
