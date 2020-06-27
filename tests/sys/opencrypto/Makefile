# $FreeBSD: stable/11/tests/sys/opencrypto/Makefile 350209 2019-07-22 08:27:44Z lwhsu $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/opencrypto
BINDIR=		${TESTSDIR}

PLAIN_TESTS_SH=	runtests

TEST_METADATA.runtests+= required_programs="python2"
TEST_METADATA.runtests+= required_user="root"

PYMODULES=	cryptodev.py cryptodevh.py cryptotest.py dpkt.py

${PACKAGE}FILES+=	${PYMODULES}

.include <bsd.test.mk>
