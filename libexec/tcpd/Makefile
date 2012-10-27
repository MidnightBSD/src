# $MidnightBSD: src/libexec/tcpd/Makefile,v 1.3 2012/04/11 01:00:19 laffer1 Exp $
# $FreeBSD: src/libexec/tcpd/Makefile,v 1.4.2.1 2005/11/16 08:01:07 ru Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/tcp_wrappers

PROG=	tcpd
MAN=	tcpd.8
CSTD?=	c89
CFLAGS+=-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DFACILITY=LOG_DAEMON
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

WARNS?=	1

.include <bsd.prog.mk>
