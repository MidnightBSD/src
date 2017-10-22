# $FreeBSD: release/10.0.0/sbin/ipfw/Makefile 204869 2010-03-08 14:43:55Z luigi $

PROG=	ipfw
SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c altq.c
WARNS?=	2
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	ipfw.8

.include <bsd.prog.mk>
