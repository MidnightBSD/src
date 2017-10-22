# $FreeBSD: release/10.0.0/lib/libproc/test/t3-name2sym/Makefile 210688 2010-07-31 16:10:20Z rpaulo $

PROG=	t3-name2sym

SRCS=	t3-name2sym.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

WITHOUT_MAN=

.include <bsd.prog.mk>
