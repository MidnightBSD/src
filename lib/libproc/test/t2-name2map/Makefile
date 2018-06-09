# $MidnightBSD$
# $FreeBSD: stable/10/lib/libproc/test/t2-name2map/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	t2-name2map

SRCS=	t2-name2map.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

MAN=

.include <bsd.prog.mk>
