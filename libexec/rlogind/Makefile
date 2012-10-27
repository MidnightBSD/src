#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rlogind/Makefile,v 1.20 2005/01/28 16:08:05 ru Exp $
# $MidnightBSD: src/libexec/rlogind/Makefile,v 1.2 2012/04/11 00:46:55 laffer1 Exp $

PROG=	rlogind
MAN=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	2

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
