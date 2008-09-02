# $MidnightBSD$
# $NetBSD: Makefile,v 1.2 2003/01/22 02:56:30 lukem Exp $

.include <bsd.own.mk>

PROG=	progress
SRCS=	progress.c progressbar.c strsuftoll.c

CFLAGS+=-I${.CURDIR}/../../contrib/lukemftp/src -DSTANDALONE_PROGRESS

.PATH:  ${.CURDIR}/../../contrib/lukemftp/src

.include <bsd.prog.mk>
