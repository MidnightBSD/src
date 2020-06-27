# $FreeBSD: stable/11/tests/sys/pjdfstest/tests/pjdfstest.test.mk 274016 2014-11-03 07:18:42Z ngie $

PJDFSTEST_SRCDIR=	${.CURDIR:H:H:H:H:H}/contrib/pjdfstest

.PATH: ${PJDFSTEST_SRCDIR}/tests/${.CURDIR:T}

TESTSDIR?=	${TESTSBASE}/sys/pjdfstest/${.CURDIR:T}

.for s in ${TAP_TESTS_SH}
TAP_TESTS_SH_SRC_$s=	$s.t
TEST_METADATA.$s+=	required_user="root"
.endfor

.include <bsd.test.mk>
