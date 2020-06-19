# $FreeBSD: stable/11/usr.sbin/nfsdumpstate/Makefile 346775 2019-04-27 02:13:15Z rmacklem $

.include <src.opts.mk>

PROG=	nfsdumpstate
MAN=	nfsdumpstate.8

.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
