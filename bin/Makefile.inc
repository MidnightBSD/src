#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.22 2004/12/21 09:59:40 ru Exp $
# $MidnightBSD: src/bin/Makefile.inc,v 1.2 2006/05/22 06:14:15 laffer1 Exp $

.include <bsd.own.mk>

BINDIR?=	/bin
WARNS?=		6

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
