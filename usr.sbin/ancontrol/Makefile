# $FreeBSD: stable/11/usr.sbin/ancontrol/Makefile 319119 2017-05-29 10:15:41Z ngie $

PROG=	ancontrol
MAN=	ancontrol.8

WARNS?=	3
CFLAGS+= -DANCACHE -I${SRCTOP}/sys

LIBADD=	md

.include <bsd.prog.mk>
