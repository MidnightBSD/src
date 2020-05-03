# $FreeBSD: stable/11/usr.sbin/newsyslog/Makefile 265420 2014-05-06 04:22:01Z imp $

.include <src.opts.mk>

PROG=	newsyslog
MAN=	newsyslog.8 newsyslog.conf.5
SRCS=	newsyslog.c ptimes.c

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
