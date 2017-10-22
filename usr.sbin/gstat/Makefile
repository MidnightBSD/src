# $FreeBSD: release/7.0.0/usr.sbin/gstat/Makefile 166355 2007-01-30 15:10:37Z ru $

PROG=	gstat
MAN=	gstat.8
WARNS?=	5
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} ${LIBEDIT} ${LIBCURSES}
LDADD=	-ldevstat -lkvm -lgeom -lbsdxml -lsbuf -ledit -lcurses

.include <bsd.prog.mk>
