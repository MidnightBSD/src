# $FreeBSD: stable/11/sbin/devd/tests/Makefile 320747 2017-07-06 17:34:53Z asomers $

ATF_TESTS_C=	client_test
TEST_METADATA.client_test=	required_files="/var/run/devd.pid"
TEST_METADATA.client_test+=	required_programs="devd"
TEST_METADATA.client_test+=	required_user="root"
TEST_METADATA.client_test+=	timeout=15

WARNS?=	5

.include <bsd.test.mk>
