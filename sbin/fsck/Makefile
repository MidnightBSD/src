#	$NetBSD: Makefile,v 1.14 1996/09/27 22:38:37 christos Exp $
# $FreeBSD: src/sbin/fsck/Makefile,v 1.11 2003/10/29 16:10:17 trhodes Exp $
# $MidnightBSD$

PROG=	fsck
SRCS=	fsck.c fsutil.c preen.c
WARNS?=	5
MAN=	fsck.8

LDADD+=-lutil
DPADD+=${LIBUTIL}

.include <bsd.prog.mk>
