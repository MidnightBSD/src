# $MidnightBSD$
#	$NetBSD: Makefile,v 1.2 1995/07/03 21:24:39 cgd Exp $
# $FreeBSD: stable/10/usr.bin/xlint/lint2/Makefile 291819 2015-12-04 19:16:56Z bdrewery $

.PATH:	${.CURDIR}/../lint1

PROG=	lint2
SRCS=	main2.c hash.c read.c mem.c mem2.c chk.c msg.c emit.c emit2.c inittyp.c
MAN=
CFLAGS+=-I${.CURDIR}/../lint1
LINTFLAGS=-abehrz

BINDIR=	${LIBEXECDIR}

.include <bsd.prog.mk>
