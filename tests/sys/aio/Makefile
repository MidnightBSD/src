# $MidnightBSD$
# $FreeBSD: stable/10/tests/sys/aio/Makefile 282858 2015-05-13 12:09:01Z ngie $

TESTSDIR=	${TESTSBASE}/sys/aio

PLAIN_TESTS_C+=	aio_kqueue_test
PLAIN_TESTS_C+=	lio_kqueue_test
ATF_TESTS_C+=	aio_test

DPADD.aio_test+=	${LIBUTIL}
LDADD.aio_test+=	-lutil

CFLAGS+=	-I${.CURDIR:H:H}

WARNS?=	6

.include <bsd.test.mk>
