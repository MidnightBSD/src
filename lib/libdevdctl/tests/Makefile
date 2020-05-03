# $FreeBSD: stable/11/lib/libdevdctl/tests/Makefile 313538 2017-02-10 07:32:40Z ngie $

TESTSDIR= ${TESTSBASE}/lib/libdevdctl

.PATH:	${.CURDIR:H}

PLAIN_TESTS_CXX= libdevdctl_unittest

SRCS.libdevdctl_unittest+= 	event_factory.cc	\
				libdevdctl_unittest.cc	\
				event.cc exception.cc	\
				guid.cc
CFLAGS.libdevdctl_unittest+= -I ${LOCALBASE}/include -D_THREAD_SAFE -pthread
DPADD.libdevdctl_unittest+= ${LIBDEVDCTL}
LDADD.libdevdctl_unittest+= -L ${LOCALBASE}/lib -D_THREAD_SAFE -pthread -lgtest -lgtest_main

# Googletest options
LOCALBASE?=	/usr/local

WARNS?= 3
.include <bsd.test.mk>
