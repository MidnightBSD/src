# Make `routed` for FreeBSD
# $FreeBSD: stable/11/sbin/routed/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=runtime
PROG=	routed
MAN=	routed.8
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
WARNS?=	3
LIBADD=	md

SUBDIR= rtquery

.include <bsd.prog.mk>
