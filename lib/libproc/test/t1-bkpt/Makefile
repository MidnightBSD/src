# $MidnightBSD$
# $FreeBSD: stable/10/lib/libproc/test/t1-bkpt/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	t1-bkpt

SRCS=	t1-bkpt.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

MAN=

.include <bsd.prog.mk>
