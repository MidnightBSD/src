#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/9/usr.sbin/pstat/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	pstat
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MAN=	pstat.8
MLINKS=	pstat.8 swapinfo.8

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
