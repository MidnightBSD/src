# $FreeBSD: release/7.0.0/usr.bin/ncal/Makefile 140909 2005-01-27 14:52:47Z delphij $

PROG=	ncal

DPADD=	${LIBCALENDAR}
LDADD=	-lcalendar
WARNS?=	1

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
