# $FreeBSD: stable/11/lib/libc/tests/setjmp/Makefile 299094 2016-05-04 23:20:53Z ngie $

NETBSD_ATF_TESTS_C=	setjmp_test
NETBSD_ATF_TESTS_C+=	threadjmp_test

LIBADD.t_threadjmp+=	pthread

WARNS?=	4

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
