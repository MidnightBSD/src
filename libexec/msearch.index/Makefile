# $MidnightBSD: src/libexec/msearch.index/Makefile,v 1.1 2011/07/24 15:14:51 laffer1 Exp $

PROG=	msearch.index

CFLAGS+=-I${.CURDIR}/../../lib/libmsearch
WARNS?=	6

NO_MAN=	yes

LDADD=	-lmsearch
DPADD=	${LIBMPORT}

.include <bsd.prog.mk>
