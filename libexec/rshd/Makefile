# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/libexec/rshd/Makefile 201380 2010-01-02 09:50:19Z ed $

PROG=	rshd
MAN=	rshd.8

WARNS?=	3
WFORMAT=0

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.include <bsd.prog.mk>
