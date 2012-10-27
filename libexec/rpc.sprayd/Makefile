# $MidnightBSD: src/libexec/rpc.sprayd/Makefile,v 1.2 2012/04/11 00:58:36 laffer1 Exp $
# $FreeBSD: src/libexec/rpc.sprayd/Makefile,v 1.6 2001/03/26 14:22:08 ru Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN =	rpc.sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

WARNS?=	2

.include <bsd.prog.mk>

