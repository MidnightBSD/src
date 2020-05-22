# $FreeBSD: stable/11/usr.bin/bmake/tests/shell/builtin/Makefile 299094 2016-05-04 23:20:53Z ngie $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/usr.bin/bmake/shell/builtin

TAP_TESTS_SH=	legacy_test

${PACKAGE}FILES+=	Makefile.test
${PACKAGE}FILES+=	expected.status.1
${PACKAGE}FILES+=	expected.status.2
${PACKAGE}FILES+=	expected.stderr.1
${PACKAGE}FILES+=	expected.stderr.2
${PACKAGE}FILES+=	expected.stdout.1
${PACKAGE}FILES+=	expected.stdout.2
${PACKAGE}FILES+=	sh

.include <bsd.test.mk>
