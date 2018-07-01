# $MidnightBSD$
# $FreeBSD: stable/10/sbin/dhclient/tests/Makefile 313488 2017-02-09 22:49:48Z ngie $

.PATH:	${.CURDIR}/..

PLAIN_TESTS_C=				option-domain-search_test
SRCS.option-domain-search_test=		alloc.c convert.c hash.c options.c \
					tables.c fake.c option-domain-search.c
CFLAGS.option-domain-search_test+=	-I${.CURDIR}/..
DPADD.option-domain-search_test=	${LIBUTIL}
LDADD.option-domain-search_test=	-lutil

WARNS?=	2

.include <bsd.test.mk>
