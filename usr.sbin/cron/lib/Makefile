# $FreeBSD: release/10.0.0/usr.sbin/cron/lib/Makefile 185042 2008-11-18 00:59:26Z matteo $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
