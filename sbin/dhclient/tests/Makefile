# $FreeBSD: stable/11/sbin/dhclient/tests/Makefile 319185 2017-05-30 04:17:22Z ngie $

.PATH:	${.CURDIR:H}

PLAIN_TESTS_C=				option-domain-search_test
SRCS.option-domain-search_test=		alloc.c convert.c hash.c options.c \
					tables.c fake.c option-domain-search.c
CFLAGS.option-domain-search_test+=	-I${.CURDIR:H}
LIBADD.option-domain-search_test=	util

WARNS?=	2

.include <bsd.test.mk>
