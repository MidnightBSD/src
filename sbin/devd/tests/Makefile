# $MidnightBSD$
# $FreeBSD: stable/10/sbin/devd/tests/Makefile 321827 2017-07-31 22:28:33Z asomers $

ATF_TESTS_C=	client_test
TEST_METADATA.client_test=	required_files="/var/run/devd.pid"
TEST_METADATA.client_test+=	required_programs="devd"
TEST_METADATA.client_test+=	required_user="root"
TEST_METADATA.client_test+=	timeout=15

WARNS?=	5

.include <bsd.test.mk>
