# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/gstat/Makefile 273499 2014-10-23 00:43:08Z ngie $

PROG=	gstat
MAN=	gstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} ${LIBEDIT} ${LIBNCURSESW}
LDADD=	-ldevstat -lkvm -lgeom -lbsdxml -lsbuf -ledit -lncursesw

.include <bsd.prog.mk>
