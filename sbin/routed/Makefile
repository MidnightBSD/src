# Make `routed` for FreeBSD
# $FreeBSD: release/7.0.0/sbin/routed/Makefile 133650 2004-08-13 14:18:24Z ru $

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
WARNS?=	0

.include <bsd.prog.mk>
