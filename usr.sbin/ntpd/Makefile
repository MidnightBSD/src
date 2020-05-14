# $MidnightBSD: src/usr.sbin/ntpd/Makefile,v 1.4 2009/11/26 18:18:55 laffer1 Exp $

NTPD_DIR=${SRCTOP}/contrib/openntpd
SYSCONFDIR=/etc

PROG=	ntpd

WARNS?=	2

.PATH: ${NTPD_DIR}
SRCS=   ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c parse.y config.c \
        server.c client.c util.c adjfreq.c arc4random.c

MAN=	ntpd.8 ntpd.conf.5
CFLAGS+=-I${NTPD_DIR} -DSYSCONFDIR=\"${SYSCONFDIR}\"

LIBADD+=	md

.include <bsd.prog.mk>

