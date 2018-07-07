# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/ncal/Makefile 264996 2014-04-27 01:15:10Z jmmv $

.include <bsd.own.mk>

PROG=	ncal

DPADD=	${LIBCALENDAR} ${LIBTERMCAP}
LDADD=	-lcalendar -ltermcap

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
