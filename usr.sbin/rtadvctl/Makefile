# $FreeBSD: stable/11/usr.sbin/rtadvctl/Makefile 319119 2017-05-29 10:15:41Z ngie $
#
.PATH:	${.CURDIR:H}/rtadvd

PROG=	rtadvctl
MAN=	rtadvctl.8

SRCS=	rtadvctl.c control.c control_client.c if.c timer_subr.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR:H}/rtadvd
WARNS?=	1

.include <bsd.prog.mk>
