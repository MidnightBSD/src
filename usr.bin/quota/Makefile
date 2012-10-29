#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $MidnightBSD$

PROG=	quota
BINOWN=	root
BINMODE=4555

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
