# $MidnightBSD$
# $FreeBSD: stable/10/tests/sys/mac/bsdextended/Makefile 293251 2016-01-06 20:02:49Z ngie $

TESTSDIR=	${TESTSBASE}/sys/mac/bsdextended

TAP_TESTS_C+=	ugidfw_test
TAP_TESTS_SH+=	matches_test

DPADD.ugidfw_test+=	${LIBUGIDFW}
LDADD.ugidfw_test+=	-lugidfw

TEST_METADATA.matches_test+=	required_user="root"
TEST_METADATA.ugidfw_test+=	required_user="root"

.include <bsd.test.mk>
