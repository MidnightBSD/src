# $FreeBSD: release/10.0.0/usr.bin/ncal/Makefile 204706 2010-03-04 12:18:24Z edwin $

PROG=	ncal

DPADD=	${LIBCALENDAR} ${LIBTERMCAP}
LDADD=	-lcalendar -ltermcap

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.include <bsd.prog.mk>
