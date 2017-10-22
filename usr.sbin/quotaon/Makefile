#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/9/usr.sbin/quotaon/Makefile 207736 2010-05-07 00:41:12Z mckusick $

PROG=	quotaon
LINKS=	${BINDIR}/quotaon ${BINDIR}/quotaoff
MAN=	quotaon.8
MLINKS=	quotaon.8 quotaoff.8

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
