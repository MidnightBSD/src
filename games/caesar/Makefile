#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: release/7.0.0/games/caesar/Makefile 145782 2005-05-01 19:34:22Z stefanf $

PROG=	caesar
DPADD=	${LIBM}
LDADD=	-lm
SCRIPTS=rot13.sh
MAN=	caesar.6
MLINKS=	caesar.6 rot13.6

WARNS?=	6

.include <bsd.prog.mk>
