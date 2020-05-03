# $FreeBSD: stable/11/lib/libcrypt/tests/Makefile 316384 2017-04-02 02:11:53Z asomers $

ATF_TESTS_C+= crypt_tests

NETBSD_ATF_TESTS_C+= crypt_test

WARNS?=		6
CFLAGS+= -I${.CURDIR:H}
LIBADD=	crypt

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
