#	$NetBSD: Makefile,v 1.14 1996/09/27 22:38:37 christos Exp $
# $FreeBSD: src/sbin/fsck/Makefile,v 1.11 2003/10/29 16:10:17 trhodes Exp $
# $MidnightBSD: src/sbin/fsck/Makefile,v 1.3 2007/01/02 06:54:22 laffer1 Exp $

PROG=	fsck
SRCS=	fsck.c fsutil.c preen.c
WARNS?=	2
MAN=	fsck.8

LDADD+=-lutil
DPADD+=${LIBUTIL}

.include <bsd.prog.mk>
