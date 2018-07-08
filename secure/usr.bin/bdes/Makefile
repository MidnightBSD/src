# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/secure/usr.bin/bdes/Makefile 126178 2004-02-23 20:25:27Z johan $

PROG=	bdes

WARNS?=	2

DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto

.include <bsd.prog.mk>
