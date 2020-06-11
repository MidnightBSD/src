# $FreeBSD: stable/11/usr.sbin/etcupdate/tests/Makefile 299094 2016-05-04 23:20:53Z ngie $

PLAIN_TESTS_SH=
.for test in	always_test \
		conflicts_test \
		fbsdid_test \
		ignore_test \
		preworld_test \
		tests_test \
		tzsetup_test
PLAIN_TESTS_SH+=	${test}
TEST_METADATA.${test}+=	required_user="root"
.endfor

.include <bsd.test.mk>
