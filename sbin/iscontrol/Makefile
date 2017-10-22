# $FreeBSD: release/10.0.0/sbin/iscontrol/Makefile 234425 2012-04-18 16:47:57Z jpaetzel $

SRCS= iscontrol.c pdu.c fsm.c config.c login.c auth_subr.c misc.c
PROG= iscontrol
DPADD= ${LIBCAM} ${LIBMD}
LDADD= -lcam -lmd
S= ${.CURDIR}/../../sys

WARNS?=	3
CFLAGS+=	-I$S

MAN= iscsi.conf.5 iscontrol.8

.include <bsd.prog.mk>
