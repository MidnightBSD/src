#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/7.0.0/usr.bin/logger/Makefile 169344 2007-05-07 11:28:01Z dwmalone $

.include <bsd.own.mk>

PROG=	logger
WARNS?=	6

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
