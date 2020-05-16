# $FreeBSD: stable/11/share/zoneinfo/tests/Makefile 360290 2020-04-25 05:51:58Z delphij $

.include <src.opts.mk>

.PATH: ${SRCTOP}/contrib/tzdata

PACKAGE=	tests

FILESGROUPS+=	TESTFILES

ATF_TESTS_SH+=	backward_test
TESTFILES+=	backward

TESTFILES+=	zoneinfo_common.sh
TESTFILESPACKAGE= ${PACKAGE}
TESTFILESDIR=	${TESTSDIR}

.include <bsd.test.mk>
