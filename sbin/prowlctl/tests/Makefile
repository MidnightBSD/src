#
# MidnightBSD prowlctl(8) / prowld(8) tests
#

PACKAGE=	tests

ATF_TESTS_SH+=	prowlctl_test notify_test trust_test

BINDIR=		${TESTSDIR}
PROGS+=		notify_helper

# Each test starts its own prowld in a scratch directory; they do not share
# state, but they do start and kill daemons, so do not run them in parallel
# with each other.
TEST_METADATA.prowlctl_test+=	is_exclusive="true"
TEST_METADATA.notify_test+=	is_exclusive="true"
TEST_METADATA.trust_test+=	is_exclusive="true"

.include <bsd.test.mk>
