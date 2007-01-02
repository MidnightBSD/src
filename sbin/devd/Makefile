# $FreeBSD: src/sbin/devd/Makefile,v 1.6 2004/12/21 09:59:42 ru Exp $

PROG_CXX=devd
SRCS=	devd.cc token.l parse.y y.tab.h
MAN=	devd.8 devd.conf.5
WARNS?=	1

NO_SHARED?=YES

DPADD=	${LIBL} ${LIBUTIL}
LDADD=	-ll -lutil

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

WARNS?=6

CLEANFILES= y.output

.include <bsd.prog.mk>
