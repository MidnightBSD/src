# $MidnightBSD$
# $FreeBSD: stable/10/tests/sys/kern/pipe/Makefile 290914 2015-11-16 05:38:40Z ngie $

TESTSDIR=	${TESTSBASE}/sys/kern/pipe

PLAIN_TESTS_C+= big_pipe_test
PLAIN_TESTS_C+= pipe_fstat_bug_test
PLAIN_TESTS_C+= pipe_ino_test
PLAIN_TESTS_C+= pipe_overcommit1_test
PLAIN_TESTS_C+= pipe_overcommit2_test
PLAIN_TESTS_C+= pipe_reverse2_test
PLAIN_TESTS_C+= pipe_reverse_test
PLAIN_TESTS_C+= pipe_wraparound_test

WARNS?=		6

.include <bsd.test.mk>
