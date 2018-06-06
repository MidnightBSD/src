# $MidnightBSD$
# $FreeBSD: stable/10/lib/libnv/tests/Makefile 313488 2017-02-09 22:49:48Z ngie $

ATF_TESTS_CXX=	\
	dnv_tests \
	nv_tests \

TAP_TESTS_C+=	nvlist_add_test
TAP_TESTS_C+=	nvlist_exists_test
TAP_TESTS_C+=	nvlist_free_test
TAP_TESTS_C+=	nvlist_get_test
TAP_TESTS_C+=	nvlist_move_test
TAP_TESTS_C+=	nvlist_send_recv_test

DPADD+=		${LIBNV}
LDADD+=		-lnv

WARNS?=		3

.include <bsd.test.mk>
