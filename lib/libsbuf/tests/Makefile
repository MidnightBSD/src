# $MidnightBSD$
# $FreeBSD: stable/10/lib/libsbuf/tests/Makefile 321113 2017-07-18 08:23:21Z ngie $

ATF_TESTS_C+=	sbuf_core_test
ATF_TESTS_C+=	sbuf_stdio_test
ATF_TESTS_C+=	sbuf_string_test

LDADD+=		-lsbuf -lutil

WARNS?=		6

.include <bsd.test.mk>
