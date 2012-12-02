# $MidnightBSD: src/usr.bin/progress/Makefile,v 1.1 2008/09/02 05:29:39 laffer1 Exp $
# $NetBSD: Makefile,v 1.2 2003/01/22 02:56:30 lukem Exp $

.include <bsd.own.mk>

PROG=	progress
SRCS=	progress.c progressbar.c strsuftoll.c

WARNS?=	2

CFLAGS+=-I${.CURDIR}/../../contrib/lukemftp/src -DSTANDALONE_PROGRESS

.PATH:  ${.CURDIR}/../../contrib/lukemftp/src

.include <bsd.prog.mk>
