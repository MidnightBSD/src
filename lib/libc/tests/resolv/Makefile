# $FreeBSD: stable/11/lib/libc/tests/resolv/Makefile 299094 2016-05-04 23:20:53Z ngie $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/lib/libc/resolv

${PACKAGE}FILES+=		mach

ATF_TESTS_C+=	resolv_test

# Note: this test relies on being dynamically linked.  You will get a
# spurious PASS for a statically linked test.
LIBADD.resolv_test+=	pthread

.include <bsd.test.mk>
