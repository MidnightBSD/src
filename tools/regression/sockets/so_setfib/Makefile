# $FreeBSD: stable/11/tools/regression/sockets/so_setfib/Makefile 281407 2015-04-11 06:38:50Z ngie $

.include <src.opts.mk>

PROG=	so_setfib
MAN=
WARNS?=	6

.if ${MK_INET} != "no"
CFLAGS+=	-DINET
.endif
.if ${MK_INET6} != "no"
CFLAGS+=	-DINET6
.endif

.include <bsd.prog.mk>
