# $FreeBSD: release/7.0.0/usr.sbin/cron/crontab/Makefile 137164 2004-11-03 18:01:21Z ru $

BINDIR=	/usr/bin

PROG=	crontab
MAN=	crontab.1 crontab.5
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

CFLAGS+= -I${.CURDIR}/../cron

DPADD=	${LIBCRON} ${LIBMD} ${LIBUTIL}
LDADD=	${LIBCRON} -lmd -lutil

.include <bsd.prog.mk>
