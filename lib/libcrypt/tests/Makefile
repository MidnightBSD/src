# $MidnightBSD$
# $FreeBSD: stable/10/lib/libcrypt/tests/Makefile 319252 2017-05-30 21:03:44Z asomers $

SRCTOP=		${.CURDIR:H:H:H}
OBJTOP=		${.OBJDIR:H:H:H}
TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libcrypt

NETBSD_ATF_TESTS_C+= crypt_test

WARNS?=		6
CFLAGS+= -I${.CURDIR:H}
DPADD+=	${LIBCRYPT}
LDADD+= -lcrypt

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
