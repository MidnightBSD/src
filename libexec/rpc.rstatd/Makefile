# $FreeBSD: release/7.0.0/libexec/rpc.rstatd/Makefile 125431 2004-02-04 10:20:43Z ru $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lrpcsvc -lutil -ldevstat -lkvm

.include <bsd.prog.mk>
