# $MidnightBSD$
# $FreeBSD: stable/10/libexec/tcpd/Makefile 321267 2017-07-20 00:52:11Z ngie $

.include <bsd.own.mk>

.PATH: ${SRCTOP}/contrib/tcp_wrappers

PROG=	tcpd
MAN=	tcpd.8
CSTD?=	c89
CFLAGS+=-DREAL_DAEMON_DIR=\"${LIBEXECDIR}\" \
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
