# $MidnightBSD: src/libexec/msearch.index/Makefile,v 1.3 2011/08/07 06:24:41 laffer1 Exp $

PROG=	msearch.index

CFLAGS+=-I${.CURDIR}/../../lib/libmsearch
WARNS?=	6
CSTD=	c99

MK_MAN= no

LIBADD= msearch z sqlite3


.include <bsd.prog.mk>
