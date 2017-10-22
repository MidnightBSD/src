# $FreeBSD: release/10.0.0/lib/libproc/test/t1-bkpt/Makefile 210688 2010-07-31 16:10:20Z rpaulo $

PROG=	t1-bkpt

SRCS=	t1-bkpt.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

WITHOUT_MAN=

.include <bsd.prog.mk>
