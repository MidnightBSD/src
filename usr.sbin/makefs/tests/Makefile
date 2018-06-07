# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/makefs/tests/Makefile 296788 2016-03-13 01:20:20Z ngie $

TESTSDIR=	${TESTSBASE}/usr.sbin/makefs

ATF_TESTS_SH+=	makefs_cd9660_tests
ATF_TESTS_SH+=	makefs_ffs_tests

BINDIR=		${TESTSDIR}

SCRIPTS+=	makefs_tests_common.sh
SCRIPTSNAME_makefs_tests_common.sh=	makefs_tests_common.sh

TEST_METADATA.makefs_cd9660_tests+=	required_files="/sbin/mount_cd9660"

.for t in ${ATF_TESTS_SH}
TEST_METADATA.$t+=	required_user="root"
.endfor

.include <bsd.test.mk>
