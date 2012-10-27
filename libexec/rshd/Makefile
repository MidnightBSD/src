# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rshd/Makefile,v 1.25 2005/01/28 16:08:05 ru Exp $
# $MidnightBSD: src/libexec/rshd/Makefile,v 1.2 2012/04/11 00:58:36 laffer1 Exp $

PROG=	rshd
MAN=	rshd.8

WARNS?=	3
WFORMAT=0

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.include <bsd.prog.mk>
