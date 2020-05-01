# $FreeBSD: stable/11/gnu/usr.bin/groff/src/devices/grops/Makefile 291738 2015-12-04 03:17:47Z bdrewery $

PROG_CXX=	grops
SRCS=		ps.cpp psrm.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF}
LDADD=		${LIBDRIVER} ${LIBGROFF}
LIBADD=		m
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
