# $FreeBSD: stable/11/usr.sbin/camdd/Makefile 319119 2017-05-29 10:15:41Z ngie $

PROG=	camdd
SRCS=	camdd.c
SDIR=	${SRCTOP}/sys
LIBADD=	cam mt util pthread
NO_WTHREAD_SAFETY=	1
MAN=	camdd.8

.include <bsd.prog.mk>
