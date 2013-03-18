# $MidnightBSD: src/usr.sbin/mport/Makefile,v 1.5 2011/05/21 19:26:24 laffer1 Exp $

PROG= mport

CC=clang
CFLAGS= -I${.CURDIR}/../../lib/libmport/

DPADD=  ${LIBMPORT}
LDADD=  -lmport

.include <bsd.prog.mk>
