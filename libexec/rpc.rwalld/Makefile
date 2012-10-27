# $MidnightBSD: src/libexec/rpc.rwalld/Makefile,v 1.2 2012/04/11 00:58:36 laffer1 Exp $
# $FreeBSD: src/libexec/rpc.rwalld/Makefile,v 1.7 2004/02/04 11:59:07 ru Exp $

PROG =	rpc.rwalld
SRCS =	rwalld.c
MAN =	rpc.rwalld.8

DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	2

.include <bsd.prog.mk>
