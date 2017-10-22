# $FreeBSD: release/10.0.0/gnu/usr.bin/groff/src/devices/grolj4/Makefile 151504 2005-10-20 10:57:53Z ru $

PROG_CXX=	grolj4
MAN=		grolj4.1 lj4_font.5
SRCS=		lj4.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
