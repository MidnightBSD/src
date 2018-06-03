# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.sbin/syslogd/Makefile 314822 2017-03-07 02:01:32Z ngie $

.include <bsd.own.mk>

.PATH: ${SRCTOP}/usr.bin/wall

PROG=	syslogd
MAN=	syslog.conf.5 syslogd.8
SRCS=	syslogd.c ttymsg.c

DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?= 3

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

CFLAGS+= -I${SRCTOP}/usr.bin/wall

.include <bsd.prog.mk>
