# $MidnightBSD$
# $FreeBSD: stable/10/lib/libmp/tests/Makefile 319252 2017-05-30 21:03:44Z asomers $

TAP_TESTS_C+=	legacy_test

DPADD+=		${LIBCRYPTO} ${LIBMP}
LDADD+=		-lcrypto -lmp
WARNS?=		3

.include <bsd.test.mk>
