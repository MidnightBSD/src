# $FreeBSD: release/10.0.0/tools/regression/sbin/dhclient/Makefile 253386 2013-07-16 09:40:59Z kevlo $

.PATH:	${.CURDIR}/../../../../sbin/dhclient

SRCS=	alloc.c convert.c hash.c options.c tables.c			\
	fake.c								\
	option-domain-search.c

CFLAGS+= -I${.CURDIR}/../../../../sbin/dhclient
LDADD=	-lutil

PROG=	option-domain-search

NO_MAN=
WARNS?=	2

.include <bsd.prog.mk>
