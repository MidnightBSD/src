# $FreeBSD: release/7.0.0/usr.sbin/cxgbtool/Makefile 167518 2007-03-14 02:51:57Z kmacy $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL

install:

.include <bsd.prog.mk>