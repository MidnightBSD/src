# $FreeBSD: release/10.0.0/libexec/rpc.rstatd/Makefile 204352 2010-02-26 09:41:16Z ru $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lrpcsvc -lutil -ldevstat -lkvm

WARNS?=	1

.include <bsd.prog.mk>
