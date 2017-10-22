#	@(#)Makefile	8.6 (Berkeley) 5/8/95
# $FreeBSD: release/10.0.0/sbin/mount/Makefile 253433 2013-07-17 19:32:07Z rodrigc $

PROG=	mount
SRCS=	mount.c mount_fs.c getmntopts.c vfslist.c
MAN=	mount.8 mount.conf.8
# We do NOT install the getmntopts.3 man page.

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
