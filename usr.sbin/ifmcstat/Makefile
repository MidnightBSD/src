#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: stable/11/usr.sbin/ifmcstat/Makefile 279028 2015-02-19 22:42:33Z glebius $

.include <src.opts.mk>

PROG=	ifmcstat
SRCS=	ifmcstat.c printb.c

MAN=	ifmcstat.8
BINMODE= 555

WARNS?=	2

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

.include <bsd.prog.mk>
