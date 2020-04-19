#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: stable/11/bin/Makefile.inc 265420 2014-05-06 04:22:01Z imp $

.include <src.opts.mk>

BINDIR?=	/bin
WARNS?=		6

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
