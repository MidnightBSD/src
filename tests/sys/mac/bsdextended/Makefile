# $FreeBSD: stable/11/tests/sys/mac/bsdextended/Makefile 299094 2016-05-04 23:20:53Z ngie $

TESTSDIR=	${TESTSBASE}/sys/mac/bsdextended

TAP_TESTS_C+=	ugidfw_test
TAP_TESTS_SH+=	matches_test

LIBADD.ugidfw_test+=	ugidfw

TEST_METADATA.matches_test+=	required_user="root"
TEST_METADATA.ugidfw_test+=	required_user="root"

.include <bsd.test.mk>
