# $MidnightBSD: src/secure/usr.bin/netpgp/Makefile,v 1.1 2009/06/07 23:59:51 laffer1 Exp $

.include <bsd.own.mk>

PROG=	netpgp

NETPGP_SRC=     ${SRCTOP}/crypto/netpgp

CFLAGS+=-I${NETPGP_SRC}

LIBADD+=	crypto z bz2 netpgp

MAN=	netpgp.1

.include <bsd.prog.mk>

.PATH:	${NETPGP_SRC}/src/bin
