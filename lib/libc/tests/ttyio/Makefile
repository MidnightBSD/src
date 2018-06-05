# $MidnightBSD$
# $FreeBSD: stable/10/lib/libc/tests/ttyio/Makefile 313488 2017-02-09 22:49:48Z ngie $

.include <bsd.own.mk>

# TODO: ptm_test
NETBSD_ATF_TESTS_C=	ttyio_test

DPADD.ttyio_test+=	${LIBUTIL}
LDADD.ttyio_test+=	-lutil

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
