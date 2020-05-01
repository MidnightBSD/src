# $FreeBSD: stable/11/usr.sbin/mtest/Makefile 265420 2014-05-06 04:22:01Z imp $

.include <src.opts.mk>

PROG=	mtest
MAN=	mtest.8

BINMODE= 555
WARNS?=	2

# XXX This assumes INET support in the base system.
CFLAGS+=-DINET

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

.include <bsd.prog.mk>
