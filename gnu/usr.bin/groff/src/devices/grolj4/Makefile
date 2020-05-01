# $FreeBSD: stable/11/gnu/usr.bin/groff/src/devices/grolj4/Makefile 291738 2015-12-04 03:17:47Z bdrewery $

PROG_CXX=	grolj4
MAN=		grolj4.1 lj4_font.5
SRCS=		lj4.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF}
LDADD=		${LIBDRIVER} ${LIBGROFF}
LIBADD=		m
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
