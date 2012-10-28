#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/dmesg/Makefile,v 1.8 2005/01/17 13:56:46 delphij Exp $
# $MidnightBSD: src/sbin/dmesg/Makefile,v 1.2 2008/11/21 14:49:02 laffer1 Exp $

PROG=	dmesg
MAN=	dmesg.8

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
