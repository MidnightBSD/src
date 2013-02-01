# $MidnightBSD: src/usr.bin/progress/Makefile,v 1.2 2012/12/02 06:26:32 laffer1 Exp $
# $NetBSD: Makefile,v 1.2 2003/01/22 02:56:30 lukem Exp $

.include <bsd.own.mk>

PROG=	progress
SRCS=	progress.c progressbar.c strsuftoll.c

WARNS?=	2

CFLAGS+=-I${.CURDIR}/../../contrib/tnftp/src -DSTANDALONE_PROGRESS -I${.CURDIR}/../../contrib/tnftp
CFLAGS+= -I${.CURDIR}/../ftp

.PATH:  ${.CURDIR}/../../contrib/tnftp/src

.include <bsd.prog.mk>
