# $FreeBSD: stable/11/bin/ls/tests/Makefile 299094 2016-05-04 23:20:53Z ngie $

ATF_TESTS_SH+=	ls_tests
# This seems like overkill, but the idea in mind is that all of the testcases
# should be runnable as !root
TEST_METADATA.ls_tests+=	required_user="unprivileged"
TEST_METADATA.ls_tests+=	required_files="/usr/bin/awk /usr/bin/nc /usr/bin/sort"

.include <bsd.test.mk>
