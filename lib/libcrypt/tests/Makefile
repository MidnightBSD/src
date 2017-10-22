# $FreeBSD: release/10.0.0/lib/libcrypt/tests/Makefile 242713 2012-11-07 22:03:59Z sjg $

# exercise libcrypt

TESTS_C= crypt_tests

CFLAGS+= -I${.CURDIR:H}
LDADD+= -L${.OBJDIR:H} -lcrypt

.include <atf.test.mk>
