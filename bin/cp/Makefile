#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/cp/Makefile,v 1.11 2002/07/15 12:08:21 sheldonh Exp $
# $MidnightBSD: src/bin/cp/Makefile,v 1.2 2006/07/07 13:46:33 laffer1 Exp $

PROG=	cp
SRCS=	cp.c utils.c
CFLAGS+= -DVM_AND_BUFFER_CACHE_SYNCHRONIZED -D_ACL_PRIVATE

.include <bsd.prog.mk>
