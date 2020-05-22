# $FreeBSD: stable/11/usr.bin/bmake/tests/suffixes/src_wild2/Makefile 299094 2016-05-04 23:20:53Z ngie $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/usr.bin/bmake/suffixes/src_wild2

TAP_TESTS_SH=	legacy_test

${PACKAGE}FILES+=		Makefile.test
${PACKAGE}FILES+=		TEST1.a
${PACKAGE}FILES+=		TEST2.a
${PACKAGE}FILES+=		expected.status.1
${PACKAGE}FILES+=		expected.stderr.1
${PACKAGE}FILES+=		expected.stdout.1

.include <bsd.test.mk>
