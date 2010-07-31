# $MidnightBSD: src/usr.sbin/cron/lib/Makefile,v 1.2 2007/08/18 07:37:09 laffer1 Exp $
# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.7 2004/10/24 15:33:02 ru Exp $

LIB=	cron
INTERNALLIB=
SRCS= entry.c env.c misc.c

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP -DPAM

.include <bsd.lib.mk>
