# $FreeBSD: release/10.0.0/gnu/usr.bin/groff/src/devices/grolbp/Makefile 114412 2003-05-01 13:22:21Z ru $

PROG_CXX=	grolbp
SRCS=		lbp.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
