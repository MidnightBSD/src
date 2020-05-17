# $FreeBSD: stable/11/usr.bin/cut/tests/Makefile 313540 2017-02-10 07:55:39Z ngie $

PACKAGE=	tests

ATF_TESTS_SH+=	cut2_test
NETBSD_ATF_TESTS_SH=	cut_test

${PACKAGE}FILES=		d_basic.out
${PACKAGE}FILES+=		d_cut.in
${PACKAGE}FILES+=		d_dflag.out
${PACKAGE}FILES+=		d_dsflag.out
${PACKAGE}FILES+=		d_latin1.in
${PACKAGE}FILES+=		d_sflag.out
${PACKAGE}FILES+=		d_utf8.in

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
