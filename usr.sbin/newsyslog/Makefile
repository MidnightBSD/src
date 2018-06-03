# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/newsyslog/Makefile 264996 2014-04-27 01:15:10Z jmmv $

.include <bsd.own.mk>

PROG=	newsyslog
MAN=	newsyslog.8 newsyslog.conf.5
SRCS=	newsyslog.c ptimes.c

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
