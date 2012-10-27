# $MidnightBSD: src/libexec/rpc.rquotad/Makefile,v 1.2 2012/04/11 00:46:55 laffer1 Exp $
# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.6 2001/03/26 14:22:07 ru Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
