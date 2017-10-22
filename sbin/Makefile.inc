#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: stable/9/sbin/Makefile.inc 198236 2009-10-19 16:00:24Z ru $

.include <bsd.own.mk>

BINDIR?=	/sbin
WARNS?=	6

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
