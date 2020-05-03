# $FreeBSD: stable/11/lib/libcasper/services/cap_sysctl/tests/Makefile 322715 2017-08-20 06:07:40Z ngie $

TAP_TESTS_C=	sysctl_test

LIBADD+=	casper
LIBADD+=	cap_sysctl
LIBADD+=	nv

WARNS?=		3

TEST_METADATA.sysctl_test+=	required_user="root"

.include <bsd.test.mk>
