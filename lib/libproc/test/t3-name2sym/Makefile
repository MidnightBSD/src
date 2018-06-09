# $MidnightBSD$
# $FreeBSD: stable/10/lib/libproc/test/t3-name2sym/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	t3-name2sym

SRCS=	t3-name2sym.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

MAN=

.include <bsd.prog.mk>
