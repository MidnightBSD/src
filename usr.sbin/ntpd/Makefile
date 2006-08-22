# $MidnightBSD: src/usr.sbin/ntpd/Makefile,v 1.2 2006/08/21 06:17:56 laffer1 Exp $

NTPD_DIR=${.CURDIR}/../../contrib/openntpd
NTPD_CDIR=${NTPD_DIR}/openbsd-compat
SYSCONFDIR=/etc

PROG=	ntpd

.PATH: ${NTPD_CDIR}
SRCS+=	bsd-misc.c 

.PATH: ${NTPD_DIR}
SRCS+=	ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c config.c \
	server.c client.c util.c y.tab.c

MAN=	ntpd.8 ntpd.conf.5
CFLAGS+=-I${NTPD_DIR} -I${NTPD_CDIR} -DSYSCONFDIR=\"${SYSCONFDIR}\"

.include <bsd.prog.mk>

