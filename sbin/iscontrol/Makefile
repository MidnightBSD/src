# $MidnightBSD$
# $FreeBSD: stable/10/sbin/iscontrol/Makefile 277173 2015-01-14 11:29:16Z trasz $

SRCS= iscontrol.c pdu.c fsm.c config.c login.c auth_subr.c misc.c
PROG= iscontrol
DPADD= ${LIBCAM} ${LIBMD}
LDADD= -lcam -lmd
S= ${.CURDIR}/../../sys

WARNS?=	3
CFLAGS+=	-I$S

MAN= iscontrol.8

.include <bsd.prog.mk>
