# $FreeBSD: stable/11/tests/sys/netmap/Makefile 346256 2019-04-16 01:03:32Z ngie $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netmap
TEST_METADATA+=	required_user="root"
TEST_METADATA+=	is_exclusive=true

PLAIN_TESTS_C+=	ctrl-api-test

CFLAGS+=	-I${SRCTOP}/tests
LIBADD+= 	pthread

WARNS?=		6

.include <bsd.test.mk>
