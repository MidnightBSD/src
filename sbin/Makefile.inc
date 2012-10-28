#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/sbin/Makefile.inc,v 1.15 2006/03/17 18:54:30 ru Exp $
# $MidnightBSD: src/sbin/Makefile.inc,v 1.3 2008/11/11 21:29:36 laffer1 Exp $

.include <bsd.own.mk>

BINDIR?=	/sbin
WARNS?=	6

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
