#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/10.0.0/usr.sbin/trpt/Makefile 160753 2006-07-27 14:52:12Z yar $

.include <bsd.own.mk>

PROG=	trpt
MAN=	trpt.8
BINGRP=	kmem
BINMODE= 2555

WARNS?=	4

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
