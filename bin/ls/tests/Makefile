# $MidnightBSD$
# $FreeBSD: stable/10/bin/ls/tests/Makefile 313488 2017-02-09 22:49:48Z ngie $

ATF_TESTS_SH+=	ls_tests
# This seems like overkill, but the idea in mind is that all of the testcases
# should be runnable as !root
TEST_METADATA.ls_tests+=	required_user="unprivileged"
TEST_METADATA.ls_tests+=	required_files="/usr/bin/awk /usr/bin/nc /usr/bin/sort"

.include <bsd.test.mk>
