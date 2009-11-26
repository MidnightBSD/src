# $MidnightBSD: src/usr.sbin/ntpd/Makefile,v 1.3 2006/08/22 01:22:12 laffer1 Exp $

NTPD_DIR=${.CURDIR}/../../contrib/openntpd
SYSCONFDIR=/etc

PROG=	ntpd

.PATH: ${NTPD_DIR}
SRCS=   ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c parse.y config.c \
        server.c client.c util.c adjfreq.c arc4random.c

DPADD= ${LIBMD}
LDADD= -lmd

MAN=	ntpd.8 ntpd.conf.5
CFLAGS+=-I${NTPD_DIR} -DSYSCONFDIR=\"${SYSCONFDIR}\"

.include <bsd.prog.mk>

