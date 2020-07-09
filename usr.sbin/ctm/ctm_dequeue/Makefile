# $FreeBSD: stable/11/usr.sbin/ctm/ctm_dequeue/Makefile 319119 2017-05-29 10:15:41Z ngie $

.PATH: ${.CURDIR:H}/ctm_rmail

PROG=	ctm_dequeue
MAN=
SRCS=	ctm_dequeue.c error.c

CFLAGS+= -I${.CURDIR:H}/ctm_rmail

WARNS?=	1

.include <bsd.prog.mk>
