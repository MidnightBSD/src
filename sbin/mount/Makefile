#	@(#)Makefile	8.6 (Berkeley) 5/8/95
# $FreeBSD: release/7.0.0/sbin/mount/Makefile 166439 2007-02-02 23:58:10Z pjd $

PROG=	mount
SRCS=	mount.c mount_fs.c getmntopts.c vfslist.c
WARNS?=	6
MAN=	mount.8
# We do NOT install the getmntopts.3 man page.

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
