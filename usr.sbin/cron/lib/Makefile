# $FreeBSD: release/7.0.0/usr.sbin/cron/lib/Makefile 170890 2007-06-17 17:25:53Z yar $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
