# $MidnightBSD$
# $FreeBSD: stable/10/lib/libutil/tests/Makefile 319252 2017-05-30 21:03:44Z asomers $

TAP_TESTS_C+=	flopen_test
TAP_TESTS_C+=	grp_test
TAP_TESTS_C+=	humanize_number_test
TAP_TESTS_C+=	pidfile_test
TAP_TESTS_C+=	trimdomain_test
TAP_TESTS_C+=	trimdomain-nodomain_test

DPADD+=		${LIBUTIL}
LDADD+=		-lutil
WARNS?=		2

.include <bsd.test.mk>
