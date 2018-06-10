# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/camdd/Makefile 291716 2015-12-03 20:54:55Z ken $

PROG=	camdd
SRCS=	camdd.c
SDIR=	${.CURDIR}/../../sys
DPADD=	${LIBCAM} ${LIBMT} ${LIBSBUF} ${LIBBSDXML} ${LIBUTIL} ${LIBTHR}
LDADD=	-lcam -lmt -lsbuf -lbsdxml -lutil -lthr
NO_WTHREAD_SAFETY=	1
MAN=	camdd.8

.include <bsd.prog.mk>
