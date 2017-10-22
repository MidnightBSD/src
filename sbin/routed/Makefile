# Make `routed` for FreeBSD
# $FreeBSD: release/10.0.0/sbin/routed/Makefile 204405 2010-02-27 10:17:27Z uqs $

PROG=	routed
MAN=	routed.8
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
WARNS?=	3
DPADD=	${LIBMD}
LDADD=	-lmd

SUBDIR= rtquery

.include <bsd.prog.mk>
