# $FreeBSD: release/10.0.0/lib/libproc/test/t2-name2map/Makefile 210688 2010-07-31 16:10:20Z rpaulo $

PROG=	t2-name2map

SRCS=	t2-name2map.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

WITHOUT_MAN=

.include <bsd.prog.mk>
