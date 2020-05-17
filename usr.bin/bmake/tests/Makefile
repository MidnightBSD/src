# $FreeBSD: stable/11/usr.bin/bmake/tests/Makefile 299094 2016-05-04 23:20:53Z ngie $

PACKAGE=	tests

${PACKAGE}FILES+=	common.sh
${PACKAGE}FILES+=	test-new.mk

TESTS_SUBDIRS=	archives
TESTS_SUBDIRS+=	basic
TESTS_SUBDIRS+=	execution
TESTS_SUBDIRS+=	shell
TESTS_SUBDIRS+=	suffixes
TESTS_SUBDIRS+=	syntax
TESTS_SUBDIRS+=	sysmk
TESTS_SUBDIRS+=	variables

.include <bsd.test.mk>
