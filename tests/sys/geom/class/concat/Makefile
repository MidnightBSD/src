# $FreeBSD: stable/11/tests/sys/geom/class/concat/Makefile 299094 2016-05-04 23:20:53Z ngie $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/geom/class/${.CURDIR:T}

TAP_TESTS_SH+=	1_test
TAP_TESTS_SH+=	2_test

${PACKAGE}FILES+=		conf.sh

.for t in ${TAP_TESTS_SH}
TEST_METADATA.$t+=	required_user="root"
.endfor

.include <bsd.test.mk>
