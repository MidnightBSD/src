# $MidnightBSD$
# $FreeBSD: stable/10/sbin/ipfw/Makefile 263086 2014-03-12 10:45:58Z glebius $

.include <bsd.own.mk>

PROG=	ipfw
SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c
WARNS?=	2

.if ${MK_PF} != "no"
SRCS+=	altq.c
CFLAGS+=-DPF
.endif

DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	ipfw.8

.include <bsd.prog.mk>
