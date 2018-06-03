# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/tcpdmatch/Makefile 291819 2015-12-04 19:16:56Z bdrewery $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/tcp_wrappers

PROG=	tcpdmatch
MAN=	tcpdmatch.8
SRCS=	tcpdmatch.c fakelog.c inetcf.c scaffold.c

CFLAGS+=-DREAL_DAEMON_DIR=\"${LIBEXECDIR}\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

WARNS?=	0

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
