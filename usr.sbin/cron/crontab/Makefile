# $FreeBSD: stable/11/usr.sbin/cron/crontab/Makefile 319119 2017-05-29 10:15:41Z ngie $

BINDIR=	/usr/bin

PROG=	crontab
MAN=	crontab.1 crontab.5
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

WARNS?=	3

CFLAGS+= -I${.CURDIR:H}/cron

LIBADD=	cron md util

.include <bsd.prog.mk>
