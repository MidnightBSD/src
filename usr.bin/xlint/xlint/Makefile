#	$NetBSD: Makefile,v 1.2 1995/07/03 21:25:14 cgd Exp $
# $FreeBSD: release/7.0.0/usr.bin/xlint/xlint/Makefile 92813 2002-03-20 18:25:50Z ru $

.PATH:	${.CURDIR}/../lint1

PROG=	xlint
PROGNAME=	lint
SRCS=	xlint.c mem.c
MAN=	lint.1

CFLAGS+=-I${.CURDIR}/../lint1
CFLAGS+= -DPREFIX=\"${TOOLS_PREFIX}\"

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
