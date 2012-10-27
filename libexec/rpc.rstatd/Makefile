# $MidnightBSD: src/libexec/rpc.rstatd/Makefile,v 1.2 2012/04/11 00:46:15 laffer1 Exp $
# $FreeBSD: src/libexec/rpc.rstatd/Makefile,v 1.8.18.1.2.1 2008/11/25 02:59:29 kensmith Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lrpcsvc -lutil -ldevstat -lkvm

WARNS?=	1

.include <bsd.prog.mk>
