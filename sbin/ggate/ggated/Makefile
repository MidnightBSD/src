# $FreeBSD: release/10.0.0/sbin/ggate/ggated/Makefile 147887 2005-07-10 21:10:20Z pjd $

.PATH: ${.CURDIR}/../shared

PROG=	ggated
MAN=	ggated.8
SRCS=	ggated.c ggate.c

DPADD=	${LIBPTHREAD}
LDADD=	-lpthread

CFLAGS+= -I${.CURDIR}/../shared

.include <bsd.prog.mk>
