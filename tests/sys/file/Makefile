# $MidnightBSD$
# $FreeBSD: stable/10/tests/sys/file/Makefile 282067 2015-04-27 08:15:17Z ngie $

TESTSDIR=	${TESTSBASE}/sys/file

BINDIR=		${TESTSDIR}

TAP_TESTS_C+=	closefrom_test
TAP_TESTS_C+=	dup_test
TAP_TESTS_C+=	fcntlflags_test
TAP_TESTS_SH+=	flock_test
PLAIN_TESTS_C+=	ftruncate_test
PLAIN_TESTS_C+=	newfileops_on_fork_test

PROGS+=		flock_helper

DPADD.closefrom_test=	${LIBUTIL}
LDADD.closefrom_test=	-lutil

DPADD.flock_helper=	${LIBPTHREAD}
LDADD.flock_helper=	-lpthread

DPADD.newfileops_on_fork_test=	${LIBPTHREAD}
LDADD.newfileops_on_fork_test=	-lpthread

.include <bsd.test.mk>
