# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/7.0.0/libexec/rshd/Makefile 140941 2005-01-28 16:08:11Z ru $

PROG=	rshd
MAN=	rshd.8

WFORMAT=0

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.include <bsd.prog.mk>
