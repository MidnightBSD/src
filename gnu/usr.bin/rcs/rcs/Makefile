# $FreeBSD: release/10.0.0/gnu/usr.bin/rcs/rcs/Makefile 74861 2001-03-27 14:59:06Z ru $

PROG=	rcs
MAN=	rcs.1 rcsintro.1 rcsfile.5
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
