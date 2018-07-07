# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/bmake/tests/Makefile 266074 2014-05-14 18:43:13Z jmmv $

TESTSDIR=	${TESTSBASE}/usr.bin/bmake

FILESDIR=	${TESTSDIR}
FILES=		common.sh
FILES+=		test-new.mk

TESTS_SUBDIRS=	archives
TESTS_SUBDIRS+=	basic
TESTS_SUBDIRS+=	execution
TESTS_SUBDIRS+=	shell
TESTS_SUBDIRS+=	suffixes
TESTS_SUBDIRS+=	syntax
TESTS_SUBDIRS+=	sysmk
TESTS_SUBDIRS+=	variables

.include <bsd.test.mk>
