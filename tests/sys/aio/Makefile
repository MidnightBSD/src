# $FreeBSD: stable/11/tests/sys/aio/Makefile 337239 2018-08-03 14:01:32Z asomers $

TESTSDIR=	${TESTSBASE}/sys/aio

ATF_TESTS_C+=	aio_test
ATF_TESTS_C+=	lio_test
TEST_METADATA.aio_test+= timeout="30"
TEST_METADATA.lio_test+= timeout="10"
# Some lio_test testcases involve system resource limitations, so cannot run concurrently
TEST_METADATA.lio_test+= is_exclusive=true

PLAIN_TESTS_C+=	aio_kqueue_test
PLAIN_TESTS_C+=	lio_kqueue_test

LIBADD.aio_test+=	util rt
LIBADD.lio_test+=	rt

CFLAGS+=	-I${.CURDIR:H:H}

WARNS?=	6

.include <bsd.test.mk>
