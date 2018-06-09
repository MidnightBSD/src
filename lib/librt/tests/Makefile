# $MidnightBSD$
# $FreeBSD: stable/10/lib/librt/tests/Makefile 313488 2017-02-09 22:49:48Z ngie $

DPADD+=		${LIBRT}
LDADD+=		-lrt

NETBSD_ATF_TESTS_C=	sched_test
NETBSD_ATF_TESTS_C+=	sem_test

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
