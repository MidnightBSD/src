#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: release/10.0.0/bin/Makefile.inc 156813 2006-03-17 18:54:44Z ru $

.include <bsd.own.mk>

BINDIR?=	/bin
WARNS?=		6

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
