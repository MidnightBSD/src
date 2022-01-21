# $MidnightBSD: src/secure/usr.bin/netpgp/Makefile,v 1.1 2009/06/07 23:59:51 laffer1 Exp $

.include <bsd.own.mk>

PROG=	netpgpkeys

NETPGP_SRC=     ${SRCTOP}/crypto/netpgp

CFLAGS+=-I${NETPGP_SRC} -I${NETPGP_SRC}/src/libmj

LIBADD+=	crypto z bz2 mj netpgp

MAN=	netpgpkeys.1

.include <bsd.prog.mk>

.PATH:	${NETPGP_SRC}/src/netpgpkeys
