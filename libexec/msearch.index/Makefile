# $MidnightBSD$

PROG=	msearch.index

CFLAGS+=-I${.CURDIR}/../../lib/libmsearch -g
WARNS?=	6

NO_MAN=	yes

LDADD=	-lmsearch
DPADD=	${LIBMPORT}

.include <bsd.prog.mk>
