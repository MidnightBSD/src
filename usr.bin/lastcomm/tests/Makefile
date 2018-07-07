# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/lastcomm/tests/Makefile 313488 2017-02-09 22:49:48Z ngie $

TAP_TESTS_SH=	legacy_test
TEST_METADATA.legacy_test+=	allowed_architectures="amd64 i386 sparc64"

FILESDIR=	${TESTSDIR}
FILES=		v1-amd64-acct.in
FILES+=		v1-amd64.out
FILES+=		v1-i386-acct.in
FILES+=		v1-i386.out
FILES+=		v1-sparc64-acct.in
FILES+=		v1-sparc64.out
FILES+=		v2-amd64-acct.in
FILES+=		v2-amd64.out
FILES+=		v2-i386-acct.in
FILES+=		v2-i386.out
FILES+=		v2-sparc64-acct.in
FILES+=		v2-sparc64.out

.include <bsd.test.mk>
