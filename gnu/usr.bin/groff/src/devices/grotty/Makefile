# $FreeBSD: stable/11/gnu/usr.bin/groff/src/devices/grotty/Makefile 291738 2015-12-04 03:17:47Z bdrewery $

PROG_CXX=	grotty
SRCS=		tty.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF}
LDADD=		${LIBDRIVER} ${LIBGROFF}
LIBADD=		m
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
