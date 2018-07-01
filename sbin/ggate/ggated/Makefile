# $MidnightBSD$
# $FreeBSD: stable/10/sbin/ggate/ggated/Makefile 296815 2016-03-13 23:32:59Z ngie $

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD} ${LIBUTIL}
LDADD=	-lpthread -lutil

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
