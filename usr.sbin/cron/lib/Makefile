# $FreeBSD: stable/11/usr.sbin/cron/lib/Makefile 319119 2017-05-29 10:15:41Z ngie $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

WARNS?=	3

CFLAGS+= -I${.CURDIR:H}/cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
